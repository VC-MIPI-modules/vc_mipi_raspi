#!/usr/bin/env python3
"""
Measure analogue gain effect from raw captures.

This script sets sensor controls on the VC MIPI subdevice, captures raw frames
from the video node, and compares measured brightness ratios against the gain
ratio expected from the requested milli-dB values.

Recommended for IMX178:
    - Use Y10 format for more headroom than GREY.
    - Keep exposure fixed.
    - Set black level to 0 to reduce offset bias.
    - Illuminate the scene uniformly and avoid clipping.

Example:
    v4l2-ctl -d /dev/video0 --set-fmt-video=width=3072,height=2048,pixelformat=Y10
    python3 gain_effect_test.py --device /dev/video0 --subdev /dev/v4l-subdev2 \
        --gains 0 6000 12000 18000 --exposure 10000 --blacklevel 0
"""

import argparse
import fcntl
import mmap
import math
import os
import struct
import sys
import time

import numpy as np


def _IOC(direction, type_char, nr, size):
    return (direction << 30) | (ord(type_char) << 8) | nr | (size << 16)


def _IOC_READ():
    return 2


def _IOC_WRITE():
    return 1


def _IOWR(type_char, nr, size):
    return _IOC(_IOC_READ() | _IOC_WRITE(), type_char, nr, size)


def _IOR(type_char, nr, size):
    return _IOC(_IOC_READ(), type_char, nr, size)


def _IOW(type_char, nr, size):
    return _IOC(_IOC_WRITE(), type_char, nr, size)


_CTRL_SIZE = 8
VIDIOC_G_CTRL = _IOC(3, 'V', 27, _CTRL_SIZE)
VIDIOC_S_CTRL = _IOC(3, 'V', 28, _CTRL_SIZE)

VIDIOC_QUERYCAP = _IOR('V', 0, 104)
VIDIOC_G_FMT = _IOWR('V', 4, 208)
VIDIOC_REQBUFS = _IOWR('V', 8, 20)
VIDIOC_QUERYBUF = _IOWR('V', 9, 88)
VIDIOC_QBUF = _IOWR('V', 15, 88)
VIDIOC_DQBUF = _IOWR('V', 17, 88)
VIDIOC_STREAMON = _IOW('V', 18, 4)
VIDIOC_STREAMOFF = _IOW('V', 19, 4)

V4L2_BUF_TYPE_VIDEO_CAPTURE = 1
V4L2_MEMORY_MMAP = 1

V4L2_PIX_FMT_Y10 = 0x20303159
V4L2_PIX_FMT_Y16 = 0x20363159
V4L2_PIX_FMT_GREY = 0x59455247

CID_BLACK_LEVEL = 0x0098090B
CID_EXPOSURE = 0x00980911
CID_ANALOGUE_GAIN = 0x009E0903
CID_TRIGGER_MODE = 0x0098FFF0

TRIGGER_FREE_RUN = 0


def ctrl_set(fd, ctrl_id, value):
    buf = struct.pack('Ii', ctrl_id, int(value))
    fcntl.ioctl(fd, VIDIOC_S_CTRL, bytearray(buf))


def ctrl_get(fd, ctrl_id):
    buf = bytearray(struct.pack('Ii', ctrl_id, 0))
    fcntl.ioctl(fd, VIDIOC_G_CTRL, buf)
    _, value = struct.unpack('Ii', buf)
    return value


def fourcc_to_string(fourcc):
    return ''.join(chr((fourcc >> (8 * index)) & 0xFF) for index in range(4))


class V4L2Capture:
    def __init__(self, device):
        self.device = device
        self.fd = None
        self.buffers = []
        self.width = 0
        self.height = 0
        self.pixelformat = 0

    def open(self):
        self.fd = os.open(self.device, os.O_RDWR)
        cap = bytearray(104)
        fcntl.ioctl(self.fd, VIDIOC_QUERYCAP, cap)

    def get_format(self):
        fmt = bytearray(208)
        fmt[0:4] = struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE)
        fcntl.ioctl(self.fd, VIDIOC_G_FMT, fmt)
        self.width = struct.unpack('I', fmt[8:12])[0]
        self.height = struct.unpack('I', fmt[12:16])[0]
        self.pixelformat = struct.unpack('I', fmt[16:20])[0]
        return self.width, self.height, self.pixelformat

    def request_buffers(self, count=4):
        reqbuf = bytearray(20)
        struct.pack_into('I', reqbuf, 0, count)
        struct.pack_into('I', reqbuf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', reqbuf, 8, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_REQBUFS, reqbuf)

        actual_count = struct.unpack('I', reqbuf[0:4])[0]
        for index in range(actual_count):
            buf = bytearray(88)
            struct.pack_into('I', buf, 0, index)
            struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
            struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
            fcntl.ioctl(self.fd, VIDIOC_QUERYBUF, buf)
            offset = struct.unpack('I', buf[68:72])[0]
            length = struct.unpack('I', buf[72:76])[0]
            mem = mmap.mmap(self.fd, length, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=offset)
            self.buffers.append({'mem': mem, 'length': length})

    def queue_all(self):
        for index in range(len(self.buffers)):
            self.queue_buffer(index)

    def queue_buffer(self, index):
        buf = bytearray(88)
        struct.pack_into('I', buf, 0, index)
        struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_QBUF, buf)

    def dequeue_buffer(self):
        buf = bytearray(88)
        struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_DQBUF, buf)
        index = struct.unpack('I', buf[0:4])[0]
        bytesused = struct.unpack('I', buf[8:12])[0]
        return index, bytesused

    def stream_on(self):
        fcntl.ioctl(self.fd, VIDIOC_STREAMON, struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE))

    def stream_off(self):
        if self.fd is None:
            return
        try:
            fcntl.ioctl(self.fd, VIDIOC_STREAMOFF, struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE))
        except OSError:
            pass

    def capture_frame(self):
        index, bytesused = self.dequeue_buffer()
        data = bytes(self.buffers[index]['mem'][:bytesused])
        self.queue_buffer(index)

        if self.pixelformat == V4L2_PIX_FMT_Y10:
            frame = np.frombuffer(data, dtype=np.uint16).reshape((self.height, self.width))
            return frame >> 6
        if self.pixelformat == V4L2_PIX_FMT_Y16:
            frame = np.frombuffer(data, dtype=np.uint16).reshape((self.height, self.width))
            # Some pipelines expose 10-bit mono as Y16. Keep ratios unchanged,
            # but normalize obvious left-justified 10-bit payloads for readability.
            if frame.size and int(frame.max()) > 4095:
                return frame >> 6
            return frame
        if self.pixelformat == V4L2_PIX_FMT_GREY:
            return np.frombuffer(data, dtype=np.uint8).reshape((self.height, self.width)).astype(np.uint16)
        raise RuntimeError(f'Unsupported pixel format: {fourcc_to_string(self.pixelformat)}')

    def close(self):
        self.stream_off()
        for buf in self.buffers:
            buf['mem'].close()
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None


def center_roi(frame, roi_fraction):
    height, width = frame.shape
    roi_width = max(8, int(width * roi_fraction))
    roi_height = max(8, int(height * roi_fraction))
    left = (width - roi_width) // 2
    top = (height - roi_height) // 2
    return frame[top:top + roi_height, left:left + roi_width]


def measure_signal(frame, roi_fraction):
    roi = center_roi(frame, roi_fraction).astype(np.float64)
    floor = np.percentile(frame, 1)
    roi_mean = float(np.mean(roi))
    roi_median = float(np.median(roi))
    roi_p99 = float(np.percentile(roi, 99))
    roi_max = float(np.max(roi))
    signal = max(roi_mean - floor, 1e-6)
    return {
        'floor': float(floor),
        'roi_mean': roi_mean,
        'roi_median': roi_median,
        'roi_p99': roi_p99,
        'roi_max': roi_max,
        'signal': float(signal),
    }


def estimate_full_scale(frame):
    max_value = int(frame.max())
    if max_value <= 255:
        return 255
    if max_value <= 1023:
        return 1023
    if max_value <= 4095:
        return 4095
    return 65535


def expected_ratio(delta_mdB):
    return math.pow(10.0, float(delta_mdB) / 20000.0)


def capture_measurement(cap, settle_frames, sample_frames, roi_fraction):
    for _ in range(settle_frames):
        cap.capture_frame()

    signals = []
    means = []
    medians = []
    floors = []
    p99s = []
    maxes = []
    clipped_fractions = []
    for _ in range(sample_frames):
        frame = cap.capture_frame()
        stats = measure_signal(frame, roi_fraction)
        roi = center_roi(frame, roi_fraction)
        full_scale = estimate_full_scale(frame)
        clipped_fractions.append(float(np.mean(roi >= int(full_scale * 0.98))))
        signals.append(stats['signal'])
        means.append(stats['roi_mean'])
        medians.append(stats['roi_median'])
        floors.append(stats['floor'])
        p99s.append(stats['roi_p99'])
        maxes.append(stats['roi_max'])

    return {
        'signal': float(np.mean(signals)),
        'roi_mean': float(np.mean(means)),
        'roi_median': float(np.mean(medians)),
        'floor': float(np.mean(floors)),
        'roi_p99': float(np.mean(p99s)),
        'roi_max': float(np.mean(maxes)),
        'clipped_fraction': float(np.mean(clipped_fractions)),
    }


def parse_args():
    parser = argparse.ArgumentParser(description='Measure gain effect from raw VC MIPI captures')
    parser.add_argument('--device', default='/dev/video0', help='Video device')
    parser.add_argument('--subdev', default='/dev/v4l-subdev2', help='Sensor subdevice')
    parser.add_argument('--gains', nargs='+', type=int, required=True,
                        help='Analogue gain values in mdB, e.g. 0 6000 12000 18000')
    parser.add_argument('--exposure', type=int, default=10000, help='Exposure in us')
    parser.add_argument('--blacklevel', type=int, default=0,
                        help='Black level control in driver-relative units 0..100000 (default: 0)')
    parser.add_argument('--roi-fraction', type=float, default=0.25,
                        help='Center ROI size as fraction of width/height (default: 0.25)')
    parser.add_argument('--startup-skip-frames', type=int, default=12,
                        help='Frames to discard after stream start before the first measurement (default: 12)')
    parser.add_argument('--settle-frames', type=int, default=12,
                        help='Frames to discard after each control change before measuring (default: 12)')
    parser.add_argument('--sample-frames', type=int, default=8, help='Frames to average per gain point (default: 8)')
    parser.add_argument('--baseline-before-each-gain', action='store_true',
                        help='Capture a fresh baseline at the first gain value before each non-baseline gain measurement')
    parser.add_argument('--tolerance', type=float, default=0.20,
                        help='Allowed relative error on ratio, e.g. 0.20 = 20%%')
    return parser.parse_args()


def main():
    args = parse_args()

    subdev_fd = os.open(args.subdev, os.O_RDWR)
    cap = V4L2Capture(args.device)

    try:
        print(f'Opening video device: {args.device}')
        cap.open()
        width, height, pixelformat = cap.get_format()
        print(f'Format: {width}x{height} {fourcc_to_string(pixelformat)}')
        print(f'Opening subdevice: {args.subdev}')

        ctrl_set(subdev_fd, CID_TRIGGER_MODE, TRIGGER_FREE_RUN)
        ctrl_set(subdev_fd, CID_EXPOSURE, args.exposure)
        ctrl_set(subdev_fd, CID_BLACK_LEVEL, args.blacklevel)

        print('Configured controls:')
        print(f'  exposure    = {ctrl_get(subdev_fd, CID_EXPOSURE)} us')
        print(f'  gain        = {ctrl_get(subdev_fd, CID_ANALOGUE_GAIN)} mdB')
        print(f'  blacklevel  = {ctrl_get(subdev_fd, CID_BLACK_LEVEL)}')

        cap.request_buffers()
        cap.queue_all()
        cap.stream_on()

        print(f'Skipping {args.startup_skip_frames} startup frames...')
        for _ in range(args.startup_skip_frames):
            cap.capture_frame()

        results = []
        reference_results = []
        baseline_gain = args.gains[0]
        baseline_measurement = None

        for index, gain in enumerate(args.gains):
            if index > 0 and args.baseline_before_each_gain:
                ctrl_set(subdev_fd, CID_ANALOGUE_GAIN, baseline_gain)
                baseline_applied_gain = ctrl_get(subdev_fd, CID_ANALOGUE_GAIN)
                baseline_measurement = capture_measurement(cap, args.settle_frames, args.sample_frames, args.roi_fraction)
                baseline_measurement['requested_gain'] = baseline_gain
                baseline_measurement['applied_gain'] = baseline_applied_gain

            ctrl_set(subdev_fd, CID_ANALOGUE_GAIN, gain)
            applied_gain = ctrl_get(subdev_fd, CID_ANALOGUE_GAIN)
            measurement = capture_measurement(cap, args.settle_frames, args.sample_frames, args.roi_fraction)
            measurement['requested_gain'] = gain
            measurement['applied_gain'] = applied_gain
            results.append(measurement)
            reference_results.append(baseline_measurement if baseline_measurement is not None else measurement)
            print(
                f"gain={gain:6d} mdB applied={applied_gain:6d} "
                f"mean={measurement['roi_mean']:9.2f} floor={measurement['floor']:7.2f} "
                f"signal={measurement['signal']:9.2f} p99={measurement['roi_p99']:7.2f} "
                f"clip={measurement['clipped_fraction'] * 100:5.1f}%"
            )

        if args.baseline_before_each_gain:
            print('\nRatio check against fresh baseline before each gain point:')
        else:
            print('\nRatio check against first gain point:')
        print(' requested  applied  measured_ratio  expected_ratio  rel_error  status')
        all_ok = True
        for index, result in enumerate(results[1:], start=1):
            baseline = reference_results[index]
            measured = result['signal'] / baseline['signal']
            expected = expected_ratio(result['applied_gain'] - baseline['applied_gain'])
            rel_error = abs(measured - expected) / expected if expected > 0 else 0.0
            status = 'PASS' if rel_error <= args.tolerance else 'FAIL'
            all_ok &= status == 'PASS'
            print(
                f" {result['requested_gain']:8d} {result['applied_gain']:7d} "
                f"{measured:14.3f} {expected:14.3f} {rel_error:10.3f}  {status}"
            )

        print('\nNotes:')
        print('  - blacklevel=0 is recommended for this test to reduce offset bias.')
        print('  - Use Y10 if possible; GREY clips earlier and hides ratio changes.')
        print('  - Keep the scene static and avoid saturation in the ROI.')
        if args.baseline_before_each_gain:
            print('  - Fresh-baseline mode reduces sensitivity to slow sunlight drift between gain points.')
        if baseline['signal'] < 32.0:
            print('  - Warning: baseline signal above floor is very small; ratios will be noise-sensitive.')
        if any(result['clipped_fraction'] > 0.01 for result in results):
            print('  - Warning: ROI clipping detected; higher-gain ratios are not trustworthy.')

        return 0 if all_ok else 1
    finally:
        cap.close()
        os.close(subdev_fd)


if __name__ == '__main__':
    sys.exit(main())