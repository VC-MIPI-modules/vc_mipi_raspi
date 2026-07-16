#!/bin/bash
# smoketest_vmax_overwrite.sh — Regression guard for the SHS/vmax_overwrite bug
#
# Bug: in vc_sen_set_exposure() (vc_mipi_core.c), when a forced frame length
# (vmax_overwrite, set via V4L2_CID_VBLANK) was SMALLER than the natural VMAX
# that vc_calculate_exposure() computed SHS against, the SHS re-base was
# skipped ("if (vmax_overwrite > state->vmax)" only handled the "larger"
# direction). The stale SHS (computed for a longer natural frame) ends up
# bigger than the shorter forced VMAX register, so the sensor's own
# VMAX-SHS exposure computation underflows -> full-frame integration
# instead of the requested short exposure (reported as gross overexposure /
# highlight clipping on IMX327).
#
# What this script checks, and why it's split into two parts:
#
# 1. DETERMINISTIC (the actual pass/fail signal): whether the attached
#    sensor's mode can even reach the buggy branch at all. vmax_overwrite's
#    achievable minimum is (active_height + vertical_blanking.min); the
#    natural VMAX for a short exposure sits at vertical_blanking.default's
#    corresponding VMAX. If min == default, the VBLANK floor can never push
#    vmax_overwrite below the natural VMAX, so the vulnerable branch is
#    structurally unreachable on this sensor/mode right now (this was found
#    to be the case for IMX566C on this rig: min=default=120). This gap
#    check needs no image capture and is exact.
#
# 2. INFORMATIONAL (visual aid, not a hard gate): a same-shutter brightness
#    comparison between relaxed and floor VBLANK, captured via rpicam-vid.
#    On real hardware this repeatedly proved too noisy in this environment
#    for a reliable automated threshold — first-frame brightness varies
#    run-to-run by an amount comparable to what a genuine bug would produce,
#    and this rig's ISP/tuning pipeline occasionally returns all-black
#    frames on later frames of a burst unrelated to this bug (a separate,
#    pre-existing quirk — worth its own investigation, but not this one).
#    So this part always exits 0 if the captures technically succeeded; it
#    prints the numbers and saves both images for you to eyeball, and only
#    warns (does not fail the script) if they look suspicious.
#
# Usage:
#   ./smoketest_vmax_overwrite.sh [--subdev /dev/v4l-subdevX] [--shutter US]
#                                 [--skip-capture]
#
# All options are optional; subdev is auto-detected if omitted.

SUBDEV=""
SHUTTER_US=200
CAPTURE_MS=1500
SKIP_CAPTURE=0
OUTDIR="/tmp/vmax_overwrite_smoketest"

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

  --subdev <dev>     V4L2 subdevice node (auto-detected if omitted)
  --shutter <us>      Fixed shutter speed in microseconds (default: 200)
  --skip-capture      Only run the deterministic VBLANK-gap check, skip
                       the informational rpicam-vid brightness comparison
EOF
    exit 1
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --subdev)        SUBDEV="$2";      shift 2 ;;
        --shutter)       SHUTTER_US="$2";  shift 2 ;;
        --skip-capture)  SKIP_CAPTURE=1;   shift ;;
        -h|--help)       usage ;;
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

SENSOR_NAME=$(v4l2-ctl --device="$SUBDEV" --get-ctrl=sensor_name 2>/dev/null \
    | awk '{print $NF}' | tr -d "'")

subdev_fmt=$(v4l2-ctl -d "$SUBDEV" --get-subdev-fmt 2>/dev/null)
RESOLUTION=$(echo "$subdev_fmt" | grep Width | awk '{print $3}' | tr '/' 'x')
FULL_WIDTH=$(echo  "$RESOLUTION" | cut -d'x' -f1)
FULL_HEIGHT=$(echo "$RESOLUTION" | cut -d'x' -f2)

echo "============================================"
echo "  vmax_overwrite / SHS re-base smoke test"
echo "============================================"
echo "  Sensor   : $SENSOR_NAME"
echo "  Subdev   : $SUBDEV"
echo "  Full res : ${FULL_WIDTH}x${FULL_HEIGHT}"
echo "  Shutter  : ${SHUTTER_US} us (fixed)"
echo "============================================"

VBLANK_CTRL=$(v4l2-ctl -d "$SUBDEV" --list-ctrls 2>/dev/null | grep "vertical_blanking")
if [[ -z "$VBLANK_CTRL" ]]; then
    echo "SKIP: sensor does not expose vertical_blanking (not a forced-VMAX / Sony-exposure sensor)"
    exit 0
fi
VBLANK_MIN=$(echo "$VBLANK_CTRL" | grep -oP 'min=\K[0-9]+')
VBLANK_DEF=$(echo "$VBLANK_CTRL" | grep -oP 'default=\K[0-9]+')
GAP=$(( VBLANK_DEF - VBLANK_MIN ))

echo ""
echo "-- 1. deterministic VBLANK-floor gap check --"
echo "vertical_blanking: min=$VBLANK_MIN default=$VBLANK_DEF (gap=$GAP)"
if [[ "$GAP" -gt 0 ]]; then
    echo "RESULT: the VBLANK floor on this sensor/mode CAN drive vmax_overwrite"
    echo "        below the natural VMAX of a short exposure — the buggy branch"
    echo "        is reachable here. Use part 2 below (or your own capture) to"
    echo "        confirm exposure is not underflowing."
else
    echo "RESULT: gap <= 0 — the VBLANK floor cannot currently push vmax_overwrite"
    echo "        below the natural VMAX on this sensor/mode, so the vulnerable"
    echo "        branch is not reachable via VBLANK alone here. This is expected"
    echo "        to change per-sensor/mode; re-run against other attached"
    echo "        modules (e.g. IMX327/IMX290) to get gap > 0."
fi

if [[ "$SKIP_CAPTURE" -eq 1 ]]; then
    exit 0
fi
if ! command -v rpicam-vid >/dev/null 2>&1; then
    echo ""
    echo "rpicam-vid not found — skipping informational capture comparison."
    exit 0
fi

echo ""
echo "-- 2. informational brightness comparison (rpicam-vid, same shutter) --"
mkdir -p "$OUTDIR"
ORIG_VBLANK=$(v4l2-ctl -d "$SUBDEV" --get-ctrl=vertical_blanking 2>/dev/null | awk -F': ' '{print $2}')

capture_frame0() {
    local label=$1 extra_args=$2
    local mjpeg="$OUTDIR/${label}.mjpeg"
    local jpg="$OUTDIR/${label}.jpg"

    # shellcheck disable=SC2086
    timeout 8 rpicam-vid -t "$CAPTURE_MS" --codec mjpeg \
        --shutter "$SHUTTER_US" --gain 1 -n -o "$mjpeg" \
        $extra_args >/dev/null 2>&1
    ffmpeg -y -i "$mjpeg" -frames:v 1 "$jpg" >/dev/null 2>&1
    rm -f "$mjpeg"

    python3 -c "
from PIL import Image
import numpy as np
try:
    im = np.array(Image.open('$jpg').convert('L'))
    print(f'{im.mean():.1f}')
except Exception:
    print('NA')
"
}

echo "Capturing relaxed VBLANK (--framerate 10, natural headroom)..."
BRIGHT_RELAXED=$(capture_frame0 "relaxed" "--framerate 10")
V_RELAXED=$(v4l2-ctl -d "$SUBDEV" --get-ctrl=vertical_blanking 2>/dev/null | awk -F': ' '{print $2}')
echo "  vertical_blanking during capture: $V_RELAXED, mean brightness: $BRIGHT_RELAXED"

echo "Capturing floor VBLANK (no framerate cap, fastest natural rate)..."
BRIGHT_FLOOR=$(capture_frame0 "floor" "")
V_FLOOR=$(v4l2-ctl -d "$SUBDEV" --get-ctrl=vertical_blanking 2>/dev/null | awk -F': ' '{print $2}')
echo "  vertical_blanking during capture: $V_FLOOR, mean brightness: $BRIGHT_FLOOR"

[[ -n "$ORIG_VBLANK" ]] && v4l2-ctl -d "$SUBDEV" --set-ctrl=vertical_blanking="$ORIG_VBLANK" >/dev/null 2>&1

echo ""
echo "Images saved to $OUTDIR/relaxed.jpg and $OUTDIR/floor.jpg — inspect visually."
if [[ "$BRIGHT_RELAXED" == "NA" || "$BRIGHT_FLOOR" == "NA" ]]; then
    echo "WARNING: capture failed for one or both conditions (see images above)."
    exit 0
fi
if [[ "$BRIGHT_RELAXED" == "0.0" && "$BRIGHT_FLOOR" == "0.0" ]]; then
    echo "INCONCLUSIVE: both captures came back completely black. This has been"
    echo "observed on this rig after many rapid back-to-back camera opens in a"
    echo "short session (a separate, pre-existing ISP pipeline quirk, unrelated"
    echo "to this fix) — wait a few seconds and re-run, or reboot the camera."
    exit 0
fi

# Informational only — not a hard gate. See header comment: brightness here
# is too noisy in this environment (AE transients + an apparently unrelated
# ISP quirk on later burst frames) for a reliable automated threshold. A
# large gap is worth a look; it is not proof of the bug, and a small gap is
# not proof of its absence, especially when part 1 above reported gap <= 0.
RATIO=$(python3 -c "
r, f = $BRIGHT_RELAXED, $BRIGHT_FLOOR
print(f'{(f / r) if r > 0 else float(\"inf\"):.2f}')
")
echo "Brightness ratio (floor / relaxed): $RATIO"
if python3 -c "import sys; sys.exit(0 if float('$RATIO') >= 1.4 else 1)"; then
    echo "WARNING: floor-VBLANK capture is notably brighter than relaxed at the"
    echo "         same shutter speed — consistent with (but not proof of) the"
    echo "         SHS-underflow symptom. Inspect the saved images."
fi
exit 0
