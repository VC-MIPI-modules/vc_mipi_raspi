#!/usr/bin/env python3
"""
VC MIPI Camera – V4L2 Controls Demo with OpenCV
================================================

Demonstrates how to set and read ALL V4L2 controls exposed by the VC MIPI
driver via direct ioctl calls.  OpenCV's CAP_PROP_* API only covers a small
subset of standard controls; vendor-specific controls (trigger_mode, io_mode,
frame_rate, single_trigger, binning_mode, live_roi …) have no OpenCV mapping
and MUST be accessed through ioctl directly.

Requirements
------------
    pip install opencv-python numpy

    The camera must be visible as /dev/video0 and the sensor subdevice as
    /dev/v4l-subdev2.  Verify with:
        v4l2-ctl --list-devices
        v4l2-ctl --all -d /dev/v4l-subdev2

Usage
-----
    python3 opencv_controls.py
    python3 opencv_controls.py --device /dev/video0 --subdev /dev/v4l-subdev2
    python3 opencv_controls.py --no-display        # set controls only, no window

Control reference  (v4l2-ctl --all -d /dev/v4l-subdev2)
---------------------------------------------------------
User Controls
  black_level         0x0098090b  int    min=0        max=100000      default=0
  exposure            0x00980911  int    min=1        max=103934233   default=10000   [µs]
  trigger_mode        0x0098fff0  int    min=0        max=7           default=0
  io_mode             0x0098fff1  int    min=0        max=5           default=0
  frame_rate          0x0098fff2  int    min=0        max=88888       default=0       [mHz, 0=maximum]
  single_trigger      0x0098fff3  button write-only                                   [fires one frame]
  binning_mode        0x0098fff4  int    min=0        max=4           default=0
  live_roi            0x0098fff5  int    min=0        max=999999999   default=0       [volatile]
  sensor_name         0x0098fff6  str    read-only, volatile                          [e.g. 'IMX900C']


Image Source Controls
  analogue_gain       0x009e0903  int    min=0        max=48000       default=0       [mdB]

Trigger modes (trigger_mode)
  0  Free-run / continuous streaming
  1  External trigger, active high (rising edge)
  2  Pulse-width trigger (exposure determined by pulse width)
  3  Self-trigger (software-timed, nanosecond-accurate shutter, no external signal)
  4  Single trigger  →  fire one frame with single_trigger button
  5  Sync trigger    →  master/slave multi-sensor synchronisation
  6  Stream edge trigger
  7  Stream level trigger
  Note: not all modes are supported by every sensor; see docs/trigger_mode.md.

IO modes (io_mode)
  0  Disabled              – flash off,   trigger active high
  1  Flash active high     – flash high,  trigger active high
  2  Flash active low      – flash low,   trigger active high
  3  Trigger active low    – flash off,   trigger active low
  4  Trigger low + Flash high – flash high, trigger active low
  5  Trigger + Flash low   – flash low,   trigger active low
  Note: not all modes are supported by every sensor; see docs/io_mode.md.

Live ROI encoding (live_roi)
  Value = B_LLLL_TTTT  (9-digit integer, no separators)
    TTTT  – top  offset in pixels (4 digits)
    LLLL  – left offset in pixels (4 digits)
    B     – binning flag          (1 digit,  0 or 1; not supported on all sensors)
  Example: left=320, top=240, no binning  →  live_roi = 003200240 → int 3200240
  Cropping dimensions (width × height) must be set with media-ctl before streaming.
  See docs/live_roi.md.

Binning mode (binning_mode)
  0  No binning (full resolution)
  1–4  Sensor-specific; see sensor init function vc_init_ctrl_<sensor> in driver
       source and the table in docs/binning_mode.md.
  When binning is active, adjust the capture resolution / ROI accordingly.
"""

import argparse
import fcntl
import os
import struct
import subprocess
import sys

import cv2
import numpy as np


# ---------------------------------------------------------------------------
# ioctl helpers
# ---------------------------------------------------------------------------
# Both VIDIOC_G_CTRL and VIDIOC_S_CTRL are _IOWR (direction = READ|WRITE = 3).
# struct v4l2_control { __u32 id; __s32 value; }  →  8 bytes
# ---------------------------------------------------------------------------

def _IOC(direction, type_char, nr, size):
    return (direction << 30) | (ord(type_char) << 8) | nr | (size << 16)

_CTRL_SIZE    = 8                                           # sizeof(struct v4l2_control)
VIDIOC_G_CTRL = _IOC(3, 'V', 27, _CTRL_SIZE)              # _IOWR('V', 27, v4l2_control)
VIDIOC_S_CTRL = _IOC(3, 'V', 28, _CTRL_SIZE)              # _IOWR('V', 28, v4l2_control)


def ctrl_set(fd, ctrl_id, value):
    """Write a single integer V4L2 control via ioctl."""
    buf = struct.pack('Ii', ctrl_id, int(value))
    try:
        fcntl.ioctl(fd, VIDIOC_S_CTRL, bytearray(buf))
    except OSError as e:
        print(f"  [WARN] set 0x{ctrl_id:08X} = {value} failed: {e}", file=sys.stderr)


def ctrl_get(fd, ctrl_id):
    """Read a single integer V4L2 control via ioctl. Returns None on error."""
    buf = bytearray(struct.pack('Ii', ctrl_id, 0))
    try:
        fcntl.ioctl(fd, VIDIOC_G_CTRL, buf)
        _, value = struct.unpack('Ii', buf)
        return value
    except OSError as e:
        print(f"  [WARN] get 0x{ctrl_id:08X} failed: {e}", file=sys.stderr)
        return None


def read_sensor_name(subdev='/dev/v4l-subdev2'):
    """Read the read-only sensor_name string control via v4l2-ctl subprocess."""
    try:
        result = subprocess.run(
            ['v4l2-ctl', '-d', subdev, '--get-ctrl=sensor_name'],
            capture_output=True, text=True, timeout=3
        )
        line = result.stdout.strip()
        if ':' in line:
            return line.split(':', 1)[1].strip()
    except Exception:
        pass
    return '(unknown)'


# ---------------------------------------------------------------------------
# Control IDs
# ---------------------------------------------------------------------------

# User Controls
CID_BLACK_LEVEL         = 0x0098090b
CID_EXPOSURE            = 0x00980911   # µs if libcamera disabled

CID_TRIGGER_MODE        = 0x0098fff0
CID_IO_MODE             = 0x0098fff1
CID_FRAME_RATE          = 0x0098fff2   # mHz (milli-Hertz); 0 = sensor maximum
CID_SINGLE_TRIGGER      = 0x0098fff3   # button: write-only, fires one frame
CID_BINNING_MODE        = 0x0098fff4
CID_LIVE_ROI            = 0x0098fff5   # volatile; encode as B_LLLL_TTTT (see docstring)
# CID_SENSOR_NAME       = 0x0098fff6   # Only for displaying the sensor name; read-only string, not an integer control



# Image Source Controls
CID_ANALOGUE_GAIN       = 0x009e0903   # mdB (milli-dB)

# ---------------------------------------------------------------------------
# Named constants for readability
# ---------------------------------------------------------------------------

# trigger_mode values
TRIGGER_FREE_RUN     = 0   # continuous streaming
TRIGGER_EXT_HIGH     = 1   # external, active high / rising edge
TRIGGER_PULSEWIDTH   = 2   # pulse-width (exposure = pulse width)
TRIGGER_SELF         = 3   # software self-trigger
TRIGGER_SINGLE       = 4   # single-shot via single_trigger button
TRIGGER_SYNC         = 5   # master/slave sync
TRIGGER_STREAM_EDGE  = 6
TRIGGER_STREAM_LEVEL = 7

# io_mode values
IO_DISABLED              = 0   # flash off,   trigger active high
IO_FLASH_HIGH            = 1   # flash high,  trigger active high
IO_FLASH_LOW             = 2   # flash low,   trigger active high
IO_TRIGGER_LOW           = 3   # flash off,   trigger active low
IO_TRIGGER_LOW_FLASH_HIGH = 4  # flash high,  trigger active low
IO_TRIGGER_FLASH_LOW     = 5   # flash low,   trigger active low

# binning_mode (sensor-specific from index 1 onward; see docs/binning_mode.md)
BINNING_NONE = 0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def encode_live_roi(top=0, left=0, binning=0):
    """
    Encode a live_roi value from its components.
      top, left  – pixel offsets (0–9999 each)
      binning    – 0 or 1 (not supported on all sensors)
    Returns an integer in the form B_LLLL_TTTT (see docs/live_roi.md).
    """
    return binning * 100_000_000 + left * 10_000 + top


def print_all_controls(fd, subdev):
    """Read and print every control value."""
    items = [
        ("black_level        (0x0098090b)", CID_BLACK_LEVEL),
        ("exposure [µs]      (0x00980911)", CID_EXPOSURE),
        ("trigger_mode       (0x0098fff0)", CID_TRIGGER_MODE),
        ("io_mode            (0x0098fff1)", CID_IO_MODE),
        ("frame_rate [mHz]   (0x0098fff2)", CID_FRAME_RATE),
        ("binning_mode       (0x0098fff4)", CID_BINNING_MODE),
        ("live_roi           (0x0098fff5)", CID_LIVE_ROI),
        ("analogue_gain [mdB](0x009e0903)", CID_ANALOGUE_GAIN),
    ]
    print("\n--- Current control values ---")
    for label, cid in items:
        val = ctrl_get(fd, cid)
        print(f"  {label}: {val}")
    print(f"  sensor_name        (0x0098fff6): {read_sensor_name(subdev)}")
    print()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='VC MIPI V4L2 controls demo with OpenCV capture',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--device', default='/dev/video0',
                        help='V4L2 video device (default: /dev/video0)')
    parser.add_argument('--subdev', default='/dev/v4l-subdev2',
                        help='Sensor subdevice for controls (default: /dev/v4l-subdev2)')
    parser.add_argument('--no-display', action='store_true',
                        help='Set controls and print values, then exit (no OpenCV window)')
    args = parser.parse_args()

    # ------------------------------------------------------------------
    # 1. Open the subdevice for ioctl control access
    # ------------------------------------------------------------------
    print(f"Opening subdevice: {args.subdev}")
    try:
        fd = os.open(args.subdev, os.O_RDWR)
    except OSError as e:
        sys.exit(f"Cannot open {args.subdev}: {e}")

    # ------------------------------------------------------------------
    # 2. Set controls  –  adjust values to suit your setup
    # ------------------------------------------------------------------
    print("Configuring controls ...")

    # --- User Controls ---
    ctrl_set(fd, CID_BLACK_LEVEL,      0)       # no black-level offset
    ctrl_set(fd, CID_EXPOSURE,     10000)       # 10 ms

    # Trigger / IO
    ctrl_set(fd, CID_TRIGGER_MODE, TRIGGER_FREE_RUN)
    ctrl_set(fd, CID_IO_MODE,      IO_DISABLED)
    ctrl_set(fd, CID_FRAME_RATE,   0)           # 0 = sensor maximum frame rate

    # Single trigger button – only meaningful in TRIGGER_SINGLE mode:
    #   ctrl_set(fd, CID_SINGLE_TRIGGER, 1)     # fires one frame

    # Binning (sensor-specific; see docs/binning_mode.md)
    ctrl_set(fd, CID_BINNING_MODE, BINNING_NONE)

    # Live ROI (requires prior media-ctl crop; see docs/live_roi.md)
    #   ctrl_set(fd, CID_LIVE_ROI, encode_live_roi(top=240, left=320))
    ctrl_set(fd, CID_LIVE_ROI, 0)              # 0 = disabled / full frame



    # --- Image Source Controls ---
    ctrl_set(fd, CID_ANALOGUE_GAIN,        0)  # 0 mdB = minimum gain

    # ------------------------------------------------------------------
    # 3. Read back and print all controls for verification
    # ------------------------------------------------------------------
    print_all_controls(fd, args.subdev)
    os.close(fd)

    if args.no_display:
        print("--no-display set, exiting.")
        return

    # ------------------------------------------------------------------
    # 4. Check display availability before opening any window
    # ------------------------------------------------------------------
    # opencv-python-headless has no GUI support; detect it early.
    display_available = (
        os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')
    )
    if not display_available:
        sys.exit(
            "No display detected ($DISPLAY / $WAYLAND_DISPLAY not set).\n"
            "Run with --no-display to set/read controls without a preview window,\n"
            "or connect to a graphical session (e.g. ssh -X)."
        )

    # Verify that this OpenCV build has GUI support (not opencv-python-headless).
    try:
        cv2.namedWindow('_test', cv2.WINDOW_NORMAL)
        cv2.destroyWindow('_test')
    except cv2.error:
        sys.exit(
            "OpenCV was built without GUI support (likely opencv-python-headless).\n"
            "Fix with:\n"
            "  pip uninstall opencv-python-headless\n"
            "  pip install opencv-python\n"
            "Or run with --no-display to skip the preview window."
        )

    # ------------------------------------------------------------------
    # 5. OpenCV capture loop
    # ------------------------------------------------------------------
    print(f"Opening capture device: {args.device}")
    cap = cv2.VideoCapture(args.device, cv2.CAP_V4L2)
    if not cap.isOpened():
        sys.exit(f"Cannot open {args.device}")

    print("Preview running.  Keys: q=quit  s=single_trigger (only in trigger_mode=4)")

    # Re-open fd for interactive use inside the loop
    fd2 = os.open(args.subdev, os.O_RDWR)

    while True:
        ret, frame = cap.read()
        if not ret:
            print("Frame read failed – check cable and trigger mode.", file=sys.stderr)
            break

        # Normalise frames stored as 16-bit (e.g. Y10 unpacked to 16 bit) for display
        if frame.dtype == np.uint16:
            display = (frame >> 2).astype(np.uint8)
        else:
            display = frame

        cv2.imshow('VC MIPI Camera', display)
        key = cv2.waitKey(1) & 0xFF

        if key == ord('q'):
            break
        elif key == ord('s'):
            mode = ctrl_get(fd2, CID_TRIGGER_MODE)
            if mode == TRIGGER_SINGLE:
                ctrl_set(fd2, CID_SINGLE_TRIGGER, 1)
                print("single_trigger fired")
            else:
                print(f"single_trigger ignored: trigger_mode={mode}, must be {TRIGGER_SINGLE} (TRIGGER_SINGLE)")

    os.close(fd2)
    cap.release()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()