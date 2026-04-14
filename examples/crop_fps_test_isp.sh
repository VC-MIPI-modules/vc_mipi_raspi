#!/bin/bash
# crop_fps_test_isp.sh — Measure ISP (rpicam-hello/libcamera) frame rate as crop height is reduced
#
# Mirrors crop_fps_test.sh but uses rpicam-hello instead of raw V4L2 streaming,
# so the full ISP pipeline (demosaic, tone-map, etc.) is included in the timing.
#
# How it works:
#   For each height step the sensor crop is applied via media-ctl (same as the
#   raw V4L2 test), then rpicam-hello streams for DURATION_MS milliseconds.
#   Per-frame FPS values printed by --info-text "%fps" are collected, the first
#   SKIP_FRAMES values are discarded (AEC/AGC startup transient), and the
#   remaining values are averaged.
#
# Usage:
#   ./crop_fps_test_isp.sh [OPTIONS]
#
# All options are optional; subdev and videodev are auto-detected if omitted.

SUBDEV=""
VIDEODEV=""
DURATION_MS=3000          # how long to stream per height step (ms)
SKIP_FRAMES=5             # fps samples to discard at the start (startup transient)
SHUTTER_US=1000           # fixed shutter in µs — keeps AEC from limiting FPS
                          # (set to 0 to let libcamera pick automatically)
MAX_FPS=0                 # cap framerate via rpicam-hello --framerate (0 = no cap)
                          # IMPORTANT: without a cap, libcamera's AEC stretches VMAX to
                          # allow long exposures even at short shutter, e.g. VMAX=9482
                          # for a crop height of 38 lines → only ~8.8 fps observed.
                          # Set to the sensor's expected max fps for the crop height.
FRONTEND_DEVICE="rp1-cfe-csi2_ch0"   # bcm2712; use 'unicam-image' for other platforms

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

  --subdev    <dev>   V4L2 subdevice node    (auto-detected if omitted)
  --videodev  <dev>   V4L2 video device node (auto-detected if omitted)
  --duration  <ms>    Streaming duration per height step in ms (default: 3000)
  --skip      <N>     FPS samples to skip at startup per step   (default: 5)
  --shutter   <us>    Fixed shutter speed in µs, 0 = auto       (default: 1000)
  --max-fps   <fps>   Cap framerate via rpicam-hello --framerate  (default: 0 = no cap)
                      Needed to prevent libcamera AEC from stretching VMAX and
                      artificially limiting FPS at small crop heights.
                      Example: --max-fps 200
  --frontend  <name>  Frontend entity name (default: rp1-cfe-csi2_ch0)
                      Use 'unicam-image' for bcm2711/bcm2837/rp3a0
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --subdev)    SUBDEV="$2";          shift 2 ;;
        --videodev)  VIDEODEV="$2";        shift 2 ;;
        --duration)  DURATION_MS="$2";     shift 2 ;;
        --skip)      SKIP_FRAMES="$2";     shift 2 ;;
        --shutter)   SHUTTER_US="$2";      shift 2 ;;
        --max-fps)   MAX_FPS="$2";         shift 2 ;;
        --frontend)  FRONTEND_DEVICE="$2"; shift 2 ;;
        -h|--help)   usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# ── mediabus code → V4L2 fourcc ───────────────────────────────────────────────
map_mediabus_to_fourcc() {
    case $1 in
        "0x3001") echo "BA81" ;; "0x3002") echo "GRBG" ;;
        "0x3013") echo "GBRG" ;; "0x3014") echo "RGGB" ;;
        "0x2001") echo "GREY" ;;
        "0x300e") echo "pGAA" ;; "0x300f") echo "pRAA" ;;
        "0x200a") echo "Y10P" ;; "0x300a") echo "pgAA" ;;
        "0x3007") echo "pBAA" ;; "0x3008") echo "pBCC" ;;
        "0x3010") echo "pGCC" ;; "0x3011") echo "pgCC" ;;
        "0x3012") echo "pRCC" ;; "0x2013") echo "Y12P" ;;
        "0x3019") echo "pBEE" ;; "0x301a") echo "pGEE" ;;
        "0x301b") echo "pgEE" ;; "0x301c") echo "pREE" ;;
        "0x202d") echo "Y14P" ;;
        "0x301d") echo "BYR2" ;; "0x301e") echo "GB16" ;;
        "0x301f") echo "GR16" ;; "0x3020") echo "RG16" ;;
        "0x202e") echo "Y16 " ;;
        *) echo "UNKN"; return 1 ;;
    esac
}

# ── auto-detect subdev / videodev ─────────────────────────────────────────────
if [[ -z "$SUBDEV" || -z "$VIDEODEV" ]]; then
    for mdev in /dev/media*; do
        [[ -e "$mdev" ]] || continue
        media_out=$(media-ctl -d "$mdev" -p 2>/dev/null)
        if echo "$media_out" | grep -q "vc_mipi_camera"; then
            [[ -z "$SUBDEV" ]] && \
                SUBDEV=$(echo "$media_out" | grep "vc_mipi_camera" -A 2 \
                    | grep "device node name" | awk '{print $4}' | head -1)
            [[ -z "$VIDEODEV" ]] && \
                VIDEODEV=$(echo "$media_out" | grep "$FRONTEND_DEVICE" -A 2 \
                    | grep "device node name" | awk '{print $4}' | head -1)
            break
        fi
    done
fi

[[ -z "$SUBDEV"   ]] && { echo "ERROR: subdev not found. Use --subdev";    exit 1; }
[[ -z "$VIDEODEV" ]] && { echo "ERROR: videodev not found. Use --videodev"; exit 1; }

# ── find media device for the video node ──────────────────────────────────────
MEDIADEV=""
for mdev in /dev/media*; do
    [[ -e "$mdev" ]] || continue
    if media-ctl -d "$mdev" -p 2>/dev/null | grep -q "${VIDEODEV#/dev/}"; then
        MEDIADEV="$mdev"; break
    fi
done
[[ -z "$MEDIADEV" ]] && { echo "ERROR: cannot find media device for $VIDEODEV"; exit 1; }

# ── read sensor format and native full resolution ─────────────────────────────
subdev_fmt=$(v4l2-ctl -d "$SUBDEV" --get-subdev-fmt 2>/dev/null)

MEDIABUS_CODE=$(echo "$subdev_fmt" | grep "Mediabus Code" | awk '{print $4}')
FOURCC=$(map_mediabus_to_fourcc "$MEDIABUS_CODE")
MEDIABUSFMT=$(echo "$subdev_fmt" | grep Mediabus | awk '{print $5}' \
    | tr -d '()' | cut -c 15- | xargs echo -n)

RESOLUTION=$(echo "$subdev_fmt" | grep Width | awk '{print $3}' | tr '/' 'x')
FULL_WIDTH=$(echo  "$RESOLUTION" | cut -d'x' -f1)
FULL_HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f2)

ENTITY_NAME=$(media-ctl -d "$MEDIADEV" -p 2>/dev/null \
    | grep -B 3 "${SUBDEV#/dev/}" | grep entity \
    | awk -F ': ' '{print $2}' | awk '{print $1, $2}')

SENSOR_NAME=$(v4l2-ctl --device="$SUBDEV" --get-ctrl=sensor_name 2>/dev/null \
    | awk '{print $NF}' | tr -d "'")

# ── build rpicam-hello optional arguments ────────────────────────────────────
SHUTTER_ARG=""
(( SHUTTER_US > 0 )) && SHUTTER_ARG="--shutter ${SHUTTER_US}"
FRAMERATE_ARG=""
(( MAX_FPS > 0 ))    && FRAMERATE_ARG="--framerate ${MAX_FPS}"

echo "============================================"
echo "  Crop Height FPS Test (ISP / rpicam-hello)"
echo "============================================"
echo "  Sensor   : $SENSOR_NAME"
echo "  Subdev   : $SUBDEV"
echo "  Videodev : $VIDEODEV"
echo "  Mediadev : $MEDIADEV"
echo "  Format   : $MEDIABUSFMT ($FOURCC)"
echo "  Full res : ${FULL_WIDTH}x${FULL_HEIGHT}"
echo "  Duration : ${DURATION_MS} ms per step"
echo "  Shutter  : ${SHUTTER_US} µs (0=auto)"  
echo "  Max FPS  : ${MAX_FPS} (0=no cap — AEC may stretch VMAX)"
echo "  Skip     : first ${SKIP_FRAMES} fps samples per step"
echo "============================================"

# ── apply sensor crop via media-ctl ───────────────────────────────────────────
apply_cropping() {
    local width=$1 height=$2 left=$3 top=$4

    media-ctl -d "$MEDIADEV" --set-v4l2 \
        "'$ENTITY_NAME':0[crop:(${left},${top})/${width}x${height}]" 2>/dev/null || true

    media-ctl -d "$MEDIADEV" -V \
        "'csi2':0 [fmt:${MEDIABUSFMT}/${width}x${height} field:none colorspace:srgb]" 2>/dev/null || true

    v4l2-ctl -d "$VIDEODEV" \
        --set-fmt-video=width="$width",height="$height",pixelformat="$FOURCC",colorspace=srgb \
        >/dev/null 2>&1 || true
}

# ── measure FPS via rpicam-hello ───────────────────────────────────────────────
# rpicam-hello with --info-text "%fps" -v 2 prints a bare float per frame to
# stdout (the info-text line). Collect all floats, skip the first SKIP_FRAMES,
# and average the rest.
measure_fps_isp() {
    local width=$1 height=$2
    local tmpfile
    tmpfile=$(mktemp)

    # shellcheck disable=SC2086
    rpicam-hello -n \
        --timeout "${DURATION_MS}" \
        --width   "$width" \
        --height  "$height" \
        --info-text "%fps" \
        $SHUTTER_ARG \
        $FRAMERATE_ARG \
        -v 2 \
        >"$tmpfile" 2>&1
    local rc=$?

    local all_out
    all_out=$(cat "$tmpfile")
    rm -f "$tmpfile"

    # Detect hard failure — only match fatal conditions, not libcamera log-level
    # "ERROR ..." lines (e.g. IPARPI line-length warnings are non-fatal).
    if [[ $rc -ne 0 ]] || echo "$all_out" | grep -q "terminate called\|Invalid scaling\|runtime_error"; then
        local err_line
        err_line=$(echo "$all_out" | grep -m1 "terminate called\|Invalid scaling\|runtime_error" || true)
        [[ -n "$err_line" ]] && echo "  !! ${err_line}" >&2
        echo "FAIL"
        return
    fi

    # Extract per-frame fps values: bare floats emitted by --info-text "%fps"
    mapfile -t fps_values < <(echo "$all_out" | grep -E '^[0-9]+\.[0-9]+$')

    local count=${#fps_values[@]}
    if (( count <= SKIP_FRAMES )); then
        echo "  !! Only $count fps samples collected (need > $SKIP_FRAMES)" >&2
        echo "FAIL"
        return
    fi

    # Average values after startup skip
    printf '%s\n' "${fps_values[@]:$SKIP_FRAMES}" \
        | awk '{sum+=$1; n++} END {if(n>0) printf "%.2f", sum/n; else print "FAIL"}'
}

# ── build height test list: step down by 16 lines until < 16 ────────────────
TEST_HEIGHTS=()
h=$FULL_HEIGHT
while (( h >= 16 )); do
    TEST_HEIGHTS+=("$h")
    h=$(( h - 16 ))
done

# ── run measurements ──────────────────────────────────────────────────────────
printf "\n%-14s %-12s %-16s %-10s\n" "Height (lines)" "FPS (avg)"  "Frame time (ms)" "Samples"
printf "%-14s %-12s %-16s %-10s\n"   "--------------" "---------"  "---------------" "-------"

for height in "${TEST_HEIGHTS[@]}"; do
    top=$(( (FULL_HEIGHT - height) / 2 ))
    apply_cropping "$FULL_WIDTH" "$height" 0 "$top"

    fps=$(measure_fps_isp "$FULL_WIDTH" "$height")

    if [[ "$fps" == "FAIL" ]]; then
        printf "%-14s %-12s %-16s %-10s\n" "$height" "FAIL" "N/A" "N/A"
        echo "  (stopping — smaller heights will also fail)"
        break
    else
        frame_ms=$(echo "scale=2; 1000 / $fps" | bc 2>/dev/null || echo "N/A")
        printf "%-14s %-12s %-16s\n" "$height" "$fps" "$frame_ms"
    fi
done

# ── restore original full resolution ──────────────────────────────────────────
echo ""
echo "Restoring full resolution ${FULL_WIDTH}x${FULL_HEIGHT} ..."
apply_cropping "$FULL_WIDTH" "$FULL_HEIGHT" 0 0
echo "Done."
