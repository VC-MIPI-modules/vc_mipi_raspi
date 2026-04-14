#!/bin/bash
# crop_fps_test.sh — Measure sensor frame rate as crop height is reduced
#
# Starts at the sensor's native resolution and steps the height down by
# halving at each step (full → full/2 → ... → 4 lines).
# Reports FPS and frame interval for each step.
#
# Usage:
#   ./crop_fps_test.sh [--subdev /dev/v4l-subdevX] [--videodev /dev/videoY]
#                      [--frames N] [--frontend <entity-name>]
#
# All options are optional; subdev and videodev are auto-detected if omitted.

SUBDEV=""
VIDEODEV=""
FRAMES=30
FRONTEND_DEVICE="rp1-cfe-csi2_ch0"   # bcm2712; override with --frontend for other platforms

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

  --subdev <dev>      V4L2 subdevice node   (auto-detected if omitted)
  --videodev <dev>    V4L2 video device node (auto-detected if omitted)
  --frames <N>        Frames to capture per height step (default: 30)
  --frontend <name>   Frontend entity name used on this platform
                      (default: rp1-cfe-csi2_ch0 for bcm2712)
                      Use 'unicam-image' for bcm2711/bcm2837/rp3a0
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --subdev)    SUBDEV="$2";          shift 2 ;;
        --videodev)  VIDEODEV="$2";        shift 2 ;;
        --frames)    FRAMES="$2";          shift 2 ;;
        --frontend)  FRONTEND_DEVICE="$2"; shift 2 ;;
        -h|--help)   usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# ── mediabus code → V4L2 fourcc (mirrors vc-config map_mediabus_to_fourcc) ────
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

[[ -z "$SUBDEV"   ]] && { echo "ERROR: subdev not found. Use --subdev";   exit 1; }
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

# ── read current sensor format and full resolution ────────────────────────────
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

echo "============================================"
echo "  Crop Height FPS Test"
echo "============================================"
echo "  Sensor   : $SENSOR_NAME"
echo "  Subdev   : $SUBDEV"
echo "  Videodev : $VIDEODEV"
echo "  Mediadev : $MEDIADEV"
echo "  Format   : $MEDIABUSFMT ($FOURCC)"
echo "  Full res : ${FULL_WIDTH}x${FULL_HEIGHT}"
echo "  Frames   : $FRAMES per step"
echo "============================================"

# ── apply crop/format (mirrors set_cropping in vc-config) ─────────────────────
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

# ── measure FPS by timing a fixed frame burst ──────────────────────────────────
measure_fps() {
    local frames=$1
    local t0 t1 elapsed_ms
    local tmpfile
    tmpfile=$(mktemp)

    t0=$(date +%s%3N)
    v4l2-ctl --stream-mmap --device="$VIDEODEV" --stream-count="$frames" >/dev/null 2>"$tmpfile"
    local rc=$?
    t1=$(date +%s%3N)

    local stderr_out
    stderr_out=$(cat "$tmpfile")
    rm -f "$tmpfile"

    elapsed_ms=$(( t1 - t0 ))

    # Detect failure: non-zero exit or STREAMON/QBUF error in stderr.
    # Do NOT use an elapsed_ms threshold — high-speed sensors (e.g. 16 lines)
    # can exceed 3000fps, so hundreds of frames may complete in milliseconds.
    if [[ $rc -ne 0 ]] \
        || echo "$stderr_out" | grep -q "returned -1\|Invalid argument"; then
        # Surface the actual error line for diagnosis
        local err_line
        err_line=$(echo "$stderr_out" | grep -m1 "returned -1\|Invalid argument" || true)
        [[ -n "$err_line" ]] && echo "  !! $err_line" >&2
        echo "FAIL"
        return
    fi

    (( elapsed_ms == 0 )) && elapsed_ms=1
    echo "scale=2; $frames * 1000 / $elapsed_ms" | bc
}

# ── build test height list: step down by 8 lines until < 8 ─────────────────
TEST_HEIGHTS=()
h=$FULL_HEIGHT
while (( h >= 8 )); do
    TEST_HEIGHTS+=("$h")
    h=$(( h - 8 ))
done

# ── run the measurements ──────────────────────────────────────────────────────
printf "\n%-14s %-12s %-16s\n" "Height (lines)" "FPS"  "Frame time (ms)"
printf "%-14s %-12s %-16s\n"   "--------------" "---"  "---------------"

for height in "${TEST_HEIGHTS[@]}"; do
    top=$(( (FULL_HEIGHT - height) / 2 ))   # centre crop vertically
    apply_cropping "$FULL_WIDTH" "$height" 0 "$top"

    fps=$(measure_fps "$FRAMES")

    if [[ "$fps" == "FAIL" ]]; then
        printf "%-14s %-12s %-16s\n" "$height" "FAIL" "N/A"
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
