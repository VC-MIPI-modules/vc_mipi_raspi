#!/usr/bin/env python3
"""
Capture GREY format frames using direct V4L2 API

This script captures 8-bit greyscale frames using direct V4L2.
Based on the working capture_y10.py implementation.

Requirements:
    pip install numpy

Usage:
    # Set format first
    v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=GREY
    
    # Capture frames
    python3 capture_grey.py --count 10
    python3 capture_grey.py --count 10 --save --output-dir ./frames
"""

import argparse
import fcntl
import mmap
import numpy as np
import os
import struct
import sys
from pathlib import Path

# Try to calculate proper ioctl values for the platform
def _IOC(dir, type, nr, size):
    """Calculate ioctl request code"""
    return (dir << 30) | (ord(type) << 8) | (nr << 0) | (size << 16)

def _IOC_READ():
    return 2

def _IOC_WRITE():
    return 1

def _IOWR(type, nr, size):
    return _IOC(_IOC_READ() | _IOC_WRITE(), type, nr, size)

def _IOR(type, nr, size):
    return _IOC(_IOC_READ(), type, nr, size)

def _IOW(type, nr, size):
    return _IOC(_IOC_WRITE(), type, nr, size)

# V4L2 Constants - recalculated for compatibility
VIDIOC_QUERYCAP  = _IOR('V', 0, 104)
VIDIOC_G_FMT     = _IOWR('V', 4, 208)
VIDIOC_REQBUFS   = _IOWR('V', 8, 20)
VIDIOC_QUERYBUF  = _IOWR('V', 9, 88)
VIDIOC_QBUF      = _IOWR('V', 15, 88)
VIDIOC_DQBUF     = _IOWR('V', 17, 88)
VIDIOC_STREAMON  = _IOW('V', 18, 4)
VIDIOC_STREAMOFF = _IOW('V', 19, 4)

V4L2_BUF_TYPE_VIDEO_CAPTURE = 1
V4L2_MEMORY_MMAP = 1

# Pixel formats
V4L2_PIX_FMT_GREY = 0x59455247  # 'GREY' (8-bit)


def fourcc_to_string(fourcc):
    """Convert FourCC integer to string"""
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
        """Open V4L2 device"""
        try:
            self.fd = os.open(self.device, os.O_RDWR)
            print(f"Opened: {self.device}")
            
            # Query capabilities
            cap = bytearray(104)
            try:
                fcntl.ioctl(self.fd, VIDIOC_QUERYCAP, cap)
                driver = cap[0:16].decode('utf-8', errors='ignore').rstrip('\x00')
                card = cap[16:48].decode('utf-8', errors='ignore').rstrip('\x00')
                print(f"Driver: {driver}, Card: {card}")
            except:
                pass
                
        except Exception as e:
            print(f"Error opening {self.device}: {e}")
            sys.exit(1)
    
    def get_format(self):
        """Get current format"""
        fmt = bytearray(208)
        fmt[0:4] = struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE)
        fcntl.ioctl(self.fd, VIDIOC_G_FMT, fmt)
        
        self.width = struct.unpack('I', fmt[8:12])[0]
        self.height = struct.unpack('I', fmt[12:16])[0]
        self.pixelformat = struct.unpack('I', fmt[16:20])[0]
        
        fmt_str = fourcc_to_string(self.pixelformat)
        print(f"Format: {fmt_str} ({self.width}x{self.height})")
        return fmt_str, self.width, self.height
    
    def request_buffers(self, count=4):
        """Request mmap buffers"""
        reqbuf = bytearray(20)
        struct.pack_into('I', reqbuf, 0, count)
        struct.pack_into('I', reqbuf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', reqbuf, 8, V4L2_MEMORY_MMAP)
        
        try:
            fcntl.ioctl(self.fd, VIDIOC_REQBUFS, reqbuf)
        except OSError as e:
            print(f"VIDIOC_REQBUFS failed: {e}")
            raise
        
        actual_count = struct.unpack('I', reqbuf[0:4])[0]
        print(f"Buffers: {actual_count}")
        
        if actual_count == 0:
            print("Error: No buffers allocated")
            return
        
        # Query and map buffers
        for i in range(actual_count):
            # v4l2_buffer structure for 64-bit: 88 bytes
            buf = bytearray(88)
            for j in range(88):
                buf[j] = 0
                
            # Set required fields
            struct.pack_into('I', buf, 0, i)                            # index at 0
            struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE) # type at 4
            struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)           # memory at 60
            
            try:
                fcntl.ioctl(self.fd, VIDIOC_QUERYBUF, buf)
            except OSError as e:
                print(f"VIDIOC_QUERYBUF failed for buffer {i}: {e}")
                raise
            
            # Extract buffer info
            # m.offset at 68, length at 72 (64-bit offsets)
            offset = struct.unpack('I', buf[68:72])[0]
            length = struct.unpack('I', buf[72:76])[0]
            
            if length == 0:
                print(f"Error: Buffer {i} has zero length")
                raise ValueError(f"Buffer {i} has invalid length=0")
            
            # Memory map the buffer
            try:
                mem = mmap.mmap(self.fd, length, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE, offset=offset)
            except OSError as e:
                print(f"mmap failed for buffer {i}: length={length}, offset={offset:#x}")
                raise
            
            self.buffers.append({'length': length, 'mem': mem})
        
        print(f"Mapped {len(self.buffers)} buffers")
    
    def queue_buffer(self, index):
        """Queue buffer for capture"""
        buf = bytearray(88)
        struct.pack_into('I', buf, 0, index)
        struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        fcntl.ioctl(self.fd, VIDIOC_QBUF, buf)
    
    def dequeue_buffer(self):
        """Dequeue filled buffer"""
        buf = bytearray(88)
        struct.pack_into('I', buf, 4, V4L2_BUF_TYPE_VIDEO_CAPTURE)
        struct.pack_into('I', buf, 60, V4L2_MEMORY_MMAP)
        
        fcntl.ioctl(self.fd, VIDIOC_DQBUF, buf)
        
        index = struct.unpack('I', buf[0:4])[0]
        bytesused = struct.unpack('I', buf[8:12])[0]
        
        return index, bytesused
    
    def stream_on(self):
        """Start streaming"""
        buf_type = struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE)
        fcntl.ioctl(self.fd, VIDIOC_STREAMON, buf_type)
        print("Streaming: ON\n")
    
    def stream_off(self):
        """Stop streaming"""
        if self.fd is not None:
            buf_type = struct.pack('I', V4L2_BUF_TYPE_VIDEO_CAPTURE)
            try:
                fcntl.ioctl(self.fd, VIDIOC_STREAMOFF, buf_type)
            except:
                pass
    
    def capture_frame(self):
        """Capture one frame"""
        try:
            index, bytesused = self.dequeue_buffer()
        except OSError as e:
            print(f"Failed to dequeue buffer: {e}")
            return None
        
        # Read frame data
        data = bytes(self.buffers[index]['mem'][:bytesused])
        
        # Re-queue buffer
        self.queue_buffer(index)
        
        # Convert to numpy array - GREY is 8-bit
        frame = np.frombuffer(data, dtype=np.uint8)
        frame = frame.reshape((self.height, self.width))
        
        return frame
    
    def close(self):
        """Cleanup"""
        self.stream_off()
        if self.buffers:
            for buf in self.buffers:
                buf['mem'].close()
        if self.fd is not None:
            os.close(self.fd)
        print("\nDevice closed")


def main():
    parser = argparse.ArgumentParser(
        description='Capture GREY format frames using V4L2',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument('--device', default='/dev/video0',
                       help='Video device (default: /dev/video0)')
    parser.add_argument('--count', type=int, default=10,
                       help='Number of frames (default: 10)')
    parser.add_argument('--save', action='store_true',
                       help='Save frames as .npy files')
    parser.add_argument('--output-dir', default='./frames_grey',
                       help='Output directory (default: ./frames_grey)')
    
    args = parser.parse_args()
    
    print("VC MIPI Camera - GREY Format Capture\n")
    
    if args.save:
        output_path = Path(args.output_dir)
        output_path.mkdir(parents=True, exist_ok=True)
        print(f"Output: {output_path}\n")
    
    cap = V4L2Capture(args.device)
    
    try:
        cap.open()
        fmt_str, width, height = cap.get_format()
        
        # Warn if not GREY
        if fmt_str != 'GREY':
            print(f"\n⚠ Warning: Format is '{fmt_str}', not 'GREY'")
            print("Set format: v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=GREY")
            print("Continuing anyway...\n")
        
        cap.request_buffers(4)
        
        # Queue all buffers
        for i in range(len(cap.buffers)):
            cap.queue_buffer(i)
        
        cap.stream_on()
        
        print(f"Capturing {args.count} frames...\n")
        
        frame_count = 0
        while frame_count < args.count:
            frame = cap.capture_frame()
            if frame is None:
                continue
            
            frame_count += 1
            
            # Print stats
            print(f"Frame {frame_count}: shape={frame.shape}, dtype={frame.dtype}, "
                  f"min={frame.min()}, max={frame.max()}, mean={frame.mean():.1f}")
            
            # Save frame
            if args.save:
                filename = f"{args.output_dir}/frame_{frame_count:04d}.npy"
                np.save(filename, frame)
                print(f"  Saved: {filename}")
        
        print(f"\n✓ Successfully captured {frame_count} frames")
        
    except KeyboardInterrupt:
        print("\nInterrupted")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        cap.close()


if __name__ == '__main__':
    main()
