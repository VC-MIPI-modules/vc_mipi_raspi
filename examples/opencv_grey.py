#!/usr/bin/env python3
"""
Capture GREY format frames using V4L2 and display/save with OpenCV

Requirements:
    pip install numpy opencv-python

Usage:
    # Set format first
    v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=GREY

    # Live preview
    python3 opencv_grey.py

    # Capture N frames and save as PNG
    python3 opencv_grey.py --count 10 --save --output-dir ./frames
"""

import argparse
import fcntl
import mmap
import numpy as np
import os
import struct
import sys
from pathlib import Path

import cv2


def _IOC(dir, type, nr, size):
    return (dir << 30) | (ord(type) << 8) | (nr << 0) | (size << 16)

def _IOR(type, nr, size):  return _IOC(2,     type, nr, size)
def _IOW(type, nr, size):  return _IOC(1,     type, nr, size)
def _IOWR(type, nr, size): return _IOC(2 | 1, type, nr, size)

VIDIOC_QUERYCAP  = _IOR ('V',  0, 104)
VIDIOC_G_FMT     = _IOWR('V',  4, 208)
VIDIOC_REQBUFS   = _IOWR('V',  8,  20)
VIDIOC_QUERYBUF  = _IOWR('V',  9,  88)
VIDIOC_QBUF      = _IOWR('V', 15,  88)
VIDIOC_DQBUF     = _IOWR('V', 17,  88)
VIDIOC_STREAMON  = _IOW ('V', 18,   4)
VIDIOC_STREAMOFF = _IOW ('V', 19,   4)

V4L2_BUF_TYPE_VIDEO_CAPTURE = 1
V4L2_MEMORY_MMAP = 1
V4L2_PIX_FMT_GREY = 0x59455247


def fourcc_to_string(fourcc):
    return "".join([chr((fourcc >> 8 * i) & 0xFF) for i in range(4)])


class V4L2Capture:
    def __init__(self, device='/dev/video0'):
        self.device = device
        self.fd = None
        self.buffers = []
        self.width = 0
        self.height = 0
        self.pixelformat = 0

    def open(self):
        try:
            self.fd = os.open(self.device, os.O_RDWR)
            print(f"Opened: {self.device}")
            cap = bytearray(104)
            try:
                fcntl.ioctl(self.fd, VIDIOC_QUERYCAP, cap)
                driver = cap[0:16].decode('utf-8', errors='ignore').rstrip('\x00')
                card   = cap[16:48].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"Driver: {driver}, Card: {card}")
            except Exception:
                pass
        except Exception as e:
            print(f"Error opening {self.device}: {e}")
            sys.exit(1)

    def get_format(self):
        fmt = bytearray(208)
        fmt[0:4] = struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE)
        fcntl.ioctl(self.fd, VIDIOC_G_FMT, fmt)
        self.width       = struct.unpack('I', fmt[8:12])[0]
        self.height      = struct.unpack('I', fmt[12:16])[0]
        self.pixelformat = struct.unpack('I', fmt[16:20])[0]
        fmt_str = fourcc_to_string(self.pixelformat)
        print(f"Format: {fmt_str} ({self.width}x{self.height})")
        return fmt_str, self.width, self.height

    def request_buffers(self, count=4):
        reqbuf = bytearray(20)
        struct.pack_into('I', reqbuf, 0, count)
        struct.pack_into('I', reqbuf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', reqbuf, 8, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_REQBUFS, reqbuf)
        actual_count = struct.unpack('I', reqbuf[0:4])[0]
        print(f"Buffers: {actual_count}")
        if actual_count == 0:
            raise RuntimeError("No buffers allocated")

        for i in range(actual_count):
            buf = bytearray(88)
            struct.pack_into('I', buf,  0, i)
            struct.pack_into('I', buf,  4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
            struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
            fcntl.ioctl(self.fd, VIDIOC_QUERYBUF, buf)
            offset = struct.unpack('I', buf[68:72])[0]
            length = struct.unpack('I', buf[72:76])[0]
            if length == 0:
                raise ValueError(f"Buffer {i} has zero length")
            mem = mmap.mmap(self.fd, length, mmap.MAP_SHARED,
                            mmap.PROT_READ | mmap.PROT_WRITE, offset=offset)
            self.buffers.append({'length': length, 'mem': mem})

        print(f"Mapped {len(self.buffers)} buffers")

    def queue_buffer(self, index):
        buf = bytearray(88)
        struct.pack_into('I', buf,  0, index)
        struct.pack_into('I', buf,  4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_QBUF, buf)

    def dequeue_buffer(self):
        buf = bytearray(88)
        struct.pack_into('I', buf,  4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_DQBUF, buf)
        index     = struct.unpack('I', buf[0:4])[0]
        bytesused = struct.unpack('I', buf[8:12])[0]
        return index, bytesused

    def stream_on(self):
        fcntl.ioctl(self.fd, VIDIOC_STREAMON, struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE))
        print("Streaming: ON\n")

    def stream_off(self):
        if self.fd is not None:
            try:
                fcntl.ioctl(self.fd, VIDIOC_STREAMOFF,
                            struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE))
            except Exception:
                pass

    def capture_frame(self):
        try:
            index, bytesused = self.dequeue_buffer()
        except OSError as e:
            print(f"Failed to dequeue buffer: {e}")
            return None
        data = bytes(self.buffers[index]['mem'][:bytesused])
        self.queue_buffer(index)
        frame = np.frombuffer(data, dtype=np.uint8).reshape((self.height, self.width))
        return frame

    def close(self):
        self.stream_off()
        for buf in self.buffers:
            buf['mem'].close()
        if self.fd is not None:
            os.close(self.fd)
        print("\nDevice closed")


def main():
    parser = argparse.ArgumentParser(
        description='VC MIPI GREY capture with OpenCV display',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--device', default='/dev/video0',
                        help='Video device (default: /dev/video0)')
    parser.add_argument('--count', type=int, default=0,
                        help='Frames to capture; 0 = run until q/Esc (default: 0)')
    parser.add_argument('--save', action='store_true',
                        help='Save frames as PNG files (implied when --output-dir is given)')
    parser.add_argument('--output-dir', default=None,
                        help='Output directory; providing this implies --save (default: ./frames_grey)')
    parser.add_argument('--no-display', action='store_true',
                        help='Skip OpenCV window (headless mode)')
    args = parser.parse_args()

    print("VC MIPI Camera - GREY OpenCV Example\n")

    output_path = None
    if args.save or args.output_dir is not None:
        output_path = Path(args.output_dir or './frames_grey')
        output_path.mkdir(parents=True, exist_ok=True)
        print(f"Saving to: {output_path}\n")

    cap = V4L2Capture(args.device)

    try:
        cap.open()
        fmt_str, width, height = cap.get_format()

        if fmt_str != 'GREY':
            print(f"Warning: format is '{fmt_str}', expected 'GREY'")
            print("Set it with: v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=GREY")
            print("Continuing anyway...\n")

        cap.request_buffers(4)
        for i in range(len(cap.buffers)):
            cap.queue_buffer(i)
        cap.stream_on()

        frame_count = 0
        unlimited = (args.count == 0)

        print("Press 'q' or Esc in the OpenCV window to quit." if not args.no_display else
              f"Capturing {'continuously' if unlimited else args.count} frame(s)...")
        print()

        while unlimited or frame_count < args.count:
            frame = cap.capture_frame()
            if frame is None:
                continue

            frame_count += 1
            print(f"Frame {frame_count:4d}: {frame.shape}  "
                  f"min={frame.min():3d}  max={frame.max():3d}  "
                  f"mean={frame.mean():6.1f}")

            if not args.no_display:
                # Annotate frame with basic stats
                display = cv2.cvtColor(frame, cv2.COLOR_GRAY2BGR)
                label = (f"Frame {frame_count}  "
                         f"min={frame.min()} max={frame.max()} "
                         f"mean={frame.mean():.1f}")
                cv2.putText(display, label, (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.imshow("VC MIPI - GREY", display)

                key = cv2.waitKey(1) & 0xFF
                if key in (ord('q'), 27):  # q or Esc
                    print("Quit requested")
                    break

            if output_path is not None:
                filename = output_path / f"frame_{frame_count:04d}.png"
                cv2.imwrite(str(filename), frame)
                print(f"         Saved: {filename}")

        print(f"\nCaptured {frame_count} frame(s)")

    except KeyboardInterrupt:
        print("\nInterrupted")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        cap.close()
        if not args.no_display:
            cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
