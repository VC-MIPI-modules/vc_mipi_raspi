#!/bin/bash
# smoketest_crop_hblank.sh — Regression guard for the HBLANK/crop-width bug
#
# Bug: vc_update_clk_rates() in vc_mipi_camera.c used to compute
#   hblank = hmax_output - cam->ctrl.frame.width   (full SENSOR width)
# instead of the active/cropped output width. Since hmax_output only
# depends on the selected mode (not on width), the reported HBLANK stayed
# frozen at whatever the full-width value was, no matter how far you
# cropped the width. libcamera uses HBLANK to derive line time / buffered
# pixel rate, so a stuck HBLANK caused it to misjudge achievable frame
# rate on width-cropped modes (reported by a customer as e.g. IMX296
# 640x400 out of 1440-wide holding ~49 fps instead of ~90).
#
# This was confirmed live on real hardware (IMX566C, 2848x2848 native):
# cropping width from 2848 to 1424 left "horizontal_blanking" pinned at
# 160 on the unpatched driver — it must change, because hmax_output is
# constant per mode, so hblank = hmax_output - active_width is an EXACT
# linear function of active_width. That gives an exact, deterministic
# check (no fps measurement / AE noise involved):
#
#   hblank(cropped) == hblank(full) + (full_width - cropped_width)
#
# The script also runs a short rpicam-hello burst at both widths so you
# can see the real-world fps effect end-to-end, but that part is
# informational only — the PASS/FAIL verdict comes from the exact
# arithmetic check above.
#
# Usage:
#   ./smoketest_crop_hblank.sh [--subdev /dev/v4l-subdevX] [--crop-width N]
#                              [--frontend <entity-name>] [--skip-fps]
#
# All options are optional; subdev is auto-detected if omitted.

SUBDEV=""
CROP_WIDTH=""          # default: half of full width, centred
FRONTEND_DEVICE="rp1-cfe-csi2_ch0"   # bcm2712; use 'unicam-image' for other platforms
SKIP_FPS=0
FPS_DURATION_MS=2000
SHUTTER_US=200

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

  --subdev <dev>       V4L2 subdevice node (auto-detected if omitted)
  --crop-width <N>     Width to crop to, centred (default: half of native width)
  --frontend <name>    Frontend entity name (default: rp1-cfe-csi2_ch0)
                        Use 'unicam-image' for bcm2711/bcm2837/rp3a0
  --skip-fps           Skip the informational rpicam-hello fps comparison
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --subdev)      SUBDEV="$2";      shift 2 ;;
        --crop-width)  CROP_WIDTH="$2";  shift 2 ;;
        --frontend)    FRONTEND_DEVICE="$2"; shift 2 ;;
        --skip-fps)    SKIP_FPS=1;       shift ;;
        -h|--help)     usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
done

# ── auto-detect subdev ────────────────────────────────────────────────────
if [[ -z "$SUBDEV" ]]; then
    for mdev in /dev/media*; do
        [[ -e "$mdev" ]] || continue
        media_out=$(media-ctl -d "$mdev" -p 2>/dev/null)
        if echo "$media_out" | grep -q "vc_mipi_camera"; then
            SUBDEV=$(echo "$media_out" | grep "vc_mipi_camera" -A 2 \
                | grep "device node name" | awk '{print $4}' | head -1)
            break
        fi
    done
fi
[[ -z "$SUBDEV" ]] && { echo "ERROR: subdev not found. Use --subdev"; exit 1; }

# ── find media device for this subdev ──────────────────────────────────────
MEDIADEV=""
for mdev in /dev/media*; do
    [[ -e "$mdev" ]] || continue
    if media-ctl -d "$mdev" -p 2>/dev/null | grep -q "${SUBDEV#/dev/}"; then
        MEDIADEV="$mdev"; break
    fi
done
[[ -z "$MEDIADEV" ]] && { echo "ERROR: cannot find media device for $SUBDEV"; exit 1; }

ENTITY_NAME=$(media-ctl -d "$MEDIADEV" -p 2>/dev/null \
    | grep -B 3 "${SUBDEV#/dev/}" | grep entity \
    | awk -F ': ' '{print $2}' | awk '{print $1, $2}')

SENSOR_NAME=$(v4l2-ctl --device="$SUBDEV" --get-ctrl=sensor_name 2>/dev/null \
    | awk '{print $NF}' | tr -d "'")

subdev_fmt=$(v4l2-ctl -d "$SUBDEV" --get-subdev-fmt 2>/dev/null)
RESOLUTION=$(echo "$subdev_fmt" | grep Width | awk '{print $3}' | tr '/' 'x')
FULL_WIDTH=$(echo  "$RESOLUTION" | cut -d'x' -f1)
FULL_HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f2)

[[ -z "$CROP_WIDTH" ]] && CROP_WIDTH=$(( FULL_WIDTH / 2 ))
LEFT=$(( (FULL_WIDTH - CROP_WIDTH) / 2 ))

echo "============================================"
echo "  HBLANK / crop-width smoke test"
echo "============================================"
echo "  Sensor      : $SENSOR_NAME"
echo "  Subdev      : $SUBDEV"
echo "  Mediadev    : $MEDIADEV"
echo "  Entity      : $ENTITY_NAME"
echo "  Full res    : ${FULL_WIDTH}x${FULL_HEIGHT}"
echo "  Crop to     : ${CROP_WIDTH}x${FULL_HEIGHT} (left=${LEFT})"
echo "============================================"

get_hblank() {
    v4l2-ctl -d "$SUBDEV" --get-ctrl=horizontal_blanking 2>/dev/null \
        | awk -F': ' '{print $2}'
}

HBLANK_FULL=$(get_hblank)
if [[ -z "$HBLANK_FULL" ]]; then
    echo "ERROR: sensor does not expose horizontal_blanking — not applicable"
    exit 1
fi
echo "Baseline horizontal_blanking (full width ${FULL_WIDTH}): $HBLANK_FULL"

echo "Applying width crop..."
media-ctl -d "$MEDIADEV" --set-v4l2 \
    "'$ENTITY_NAME':0[crop:(${LEFT},0)/${CROP_WIDTH}x${FULL_HEIGHT}]"
RC=$?
if [[ $RC -ne 0 ]]; then
    echo "ERROR: media-ctl crop failed (rc=$RC)"
    exit 1
fi

# vc_update_clk_rates() — which computes horizontal_blanking — only runs at
# subdev init and whenever the driver's own "frame_rate" control is written
# (see vc_ctrl_s_ctrl()'s V4L2_CID_VC_FRAME_RATE case in vc_mipi_camera.c).
# A plain crop/set_selection does NOT recompute it, so without this poke
# horizontal_blanking would just report its stale init-time value regardless
# of whether the crop-width bug is present or fixed.
v4l2-ctl -d "$SUBDEV" --set-ctrl=frame_rate=1 >/dev/null 2>&1

HBLANK_CROPPED=$(get_hblank)
EXPECTED=$(( HBLANK_FULL + (FULL_WIDTH - CROP_WIDTH) ))

echo "horizontal_blanking after crop to ${CROP_WIDTH}: $HBLANK_CROPPED"
echo "Expected (exact): $EXPECTED   (= baseline + (full_width - crop_width))"

PASS=1
if [[ "$HBLANK_CROPPED" -eq "$HBLANK_FULL" ]]; then
    echo "FAIL: horizontal_blanking did not change at all when width was cropped."
    echo "      This is exactly the bug: hblank is being computed from the full"
    echo "      sensor width instead of the active/cropped width."
    PASS=0
elif [[ "$HBLANK_CROPPED" -ne "$EXPECTED" ]]; then
    echo "FAIL: horizontal_blanking changed, but not by the expected exact amount."
    echo "      (got $HBLANK_CROPPED, expected $EXPECTED)"
    PASS=0
else
    echo "PASS: horizontal_blanking scales exactly with the active/cropped width."
fi

# ── optional informational fps comparison via rpicam-hello ──────────────────
if [[ "$SKIP_FPS" -eq 0 ]] && command -v rpicam-hello >/dev/null 2>&1; then
    echo ""
    echo "-- informational: real fps at full vs. cropped width (rpicam-hello) --"
    for w in "$FULL_WIDTH" "$CROP_WIDTH"; do
        tmpfile=$(mktemp)
        rpicam-hello -n --timeout "$FPS_DURATION_MS" --width "$w" --height "$FULL_HEIGHT" \
            --shutter "$SHUTTER_US" --info-text "%fps" -v 2 >"$tmpfile" 2>&1
        avg=$(grep -E '^[0-9]+\.[0-9]+$' "$tmpfile" | tail -n +6 \
            | awk '{sum+=$1; n++} END {if(n>0) printf "%.2f", sum/n; else print "N/A"}')
        echo "  width=${w}: avg fps (steady-state) = ${avg}"
        rm -f "$tmpfile"
    done
fi

# ── restore full width ────────────────────────────────────────────────────
echo ""
echo "Restoring full width ${FULL_WIDTH}x${FULL_HEIGHT}..."
media-ctl -d "$MEDIADEV" --set-v4l2 \
    "'$ENTITY_NAME':0[crop:(0,0)/${FULL_WIDTH}x${FULL_HEIGHT}]" >/dev/null 2>&1
v4l2-ctl -d "$SUBDEV" --set-ctrl=frame_rate=0 >/dev/null 2>&1

if [[ "$PASS" -eq 1 ]]; then
    echo "RESULT: PASS"
    exit 0
else
    echo "RESULT: FAIL"
    exit 1
fi
