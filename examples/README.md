# VC MIPI Camera - Python Examples

This directory contains Python examples for capturing raw frames from VC MIPI cameras using direct V4L2 API.

## Overview

| Format | Script | Description |
|--------|--------|-------------|
| **Y10** (10-bit) | `capture_y10.py` | 10-bit unpacked to 16-bit, high dynamic range |
| **GREY** (8-bit) | `capture_grey.py` | Standard 8-bit greyscale |

Both scripts use direct V4L2 API (no OpenCV) for proper raw format handling.

## Installation

### Step 1: Create Virtual Environment

```bash
# Navigate to examples directory
cd /home/vc/vc_mipi_raspi/examples

# Create virtual environment
python3 -m venv venv

# Activate virtual environment
source venv/bin/activate
```

### Step 2: Install Dependencies

```bash
# Install numpy (only dependency)
pip install numpy
```

### Step 3: Verify Installation

```bash
python3 -c "import numpy; print(f'NumPy {numpy.__version__} installed')"
```

**Note:** To deactivate the virtual environment when done:
```bash
deactivate
```

## Quick Start

### Capture Y10 Format (10-bit, 16-bit storage)

```bash
# Activate virtual environment
source venv/bin/activate

# Set format
v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=Y10

# Capture 10 frames
python3 capture_y10.py --count 10

# Save frames as .npy files
python3 capture_y10.py --count 100 --save --output-dir ./frames_y10
```

### Capture GREY Format (8-bit)

```bash
# Activate virtual environment
source venv/bin/activate

# Set format
v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=GREY

# Capture 10 frames
python3 capture_grey.py --count 10

# Save frames
python3 capture_grey.py --count 100 --save --output-dir ./frames_grey
```

## Scripts

### capture_y10.py

**Direct V4L2 implementation for Y10 format (10-bit unpacked to 16-bit).**

This script bypasses OpenCV to get true 10-bit data. The sensor outputs 10-bit values that are left-shifted by 6 bits into 16-bit storage.

**Features:**
- ✅ True 16-bit Y10 format support
- ✅ Direct V4L2 memory-mapped buffers
- ✅ Zero-copy frame access
- ✅ High performance
- ✅ Proper 10-bit value extraction (frame >> 6)

**Usage:**
```bash
# Basic capture
python3 capture_y10.py --count 10

# Save frames
python3 capture_y10.py --count 100 --save --output-dir ./my_frames

# Specify device
python3 capture_y10.py --device /dev/video0 --count 10
```

**Output Format:**
- NumPy array: `shape=(height, width), dtype=np.uint16`
- Value range: 0-65472 (10-bit values left-shifted by 6)
- To get 10-bit values: `value_10bit = frame >> 6`
- To get 8-bit for display: `value_8bit = frame >> 8`

### capture_grey.py

**Direct V4L2 implementation for GREY format (8-bit greyscale).**

Simplified version optimized for 8-bit capture.

**Features:**
- ✅ Native 8-bit GREY format support
- ✅ Direct V4L2 memory-mapped buffers
- ✅ Simple uint8 output
- ✅ High performance

**Usage:**
```bash
# Basic capture
python3 capture_grey.py --count 10

# Save frames
python3 capture_grey.py --count 100 --save --output-dir ./my_frames

# Specify device
python3 capture_grey.py --device /dev/video0 --count 10
```

**Output Format:**
- NumPy array: `shape=(height, width), dtype=np.uint8`
- Value range: 0-255

## Understanding Pixel Formats

### Y10 (10-bit Greyscale, Unpacked)
- **FourCC:** `Y10 ` (0x20303159)
- **Storage:** 16-bit per pixel (10-bit values left-shifted by 6)
- **Value range:** 0-65472
- **NumPy dtype:** `np.uint16`
- **Use case:** High dynamic range, low-light applications

**Working with Y10 data:**
```python
import numpy as np

# Load captured frame
frame = np.load('frame_0001.npy')  # dtype=uint16, shape=(height, width)

# Get true 10-bit values (0-1023)
frame_10bit = frame >> 6

# Convert to 8-bit for display (0-255)
frame_8bit = (frame >> 8).astype(np.uint8)

# Statistics on 10-bit data
print(f"10-bit range: {frame_10bit.min()} - {frame_10bit.max()}")
print(f"Mean: {frame_10bit.mean():.1f}")
```

### GREY (8-bit Greyscale)
- **FourCC:** `GREY` (0x59455247)
- **Storage:** 8-bit per pixel
- **Value range:** 0-255
- **NumPy dtype:** `np.uint8`
- **Use case:** Standard greyscale imaging, lower bandwidth

**Working with GREY data:**
```python
import numpy as np

# Load captured frame
frame = np.load('frame_0001.npy')  # dtype=uint8, shape=(height, width)

# Direct processing - already 8-bit
print(f"Range: {frame.min()} - {frame.max()}")
print(f"Mean: {frame.mean():.1f}")
```

## Configuring Video Device

### Check Current Settings

```bash
# List all supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Get current format
v4l2-ctl -d /dev/video0 --get-fmt-video

# Check device capabilities
v4l2-ctl -d /dev/video0 --all
```

Use vc-config for setting the format

```

### Persistent Configuration with vc-config

```bash
# Navigate to platform-specific scripts
cd /home/vc/vc_mipi_raspi/scripts/bcm2711  # or bcm2712, bcm2837, etc.

# Run configuration utility
bash ./vc-config

# Select your camera and format
# Choose to persist configuration for automatic application at boot
```

## Troubleshooting

### Virtual Environment Issues

**Problem:** `venv` command not found
```bash
# Install python3-venv
sudo apt update
sudo apt install python3-venv
```

**Problem:** Forgot to activate venv
```bash
# You'll see an import error. Activate the venv:
source venv/bin/activate

# Verify:
which python3  # Should show path inside venv/
```

### Camera Access Issues

**Problem:** Permission denied accessing /dev/video0
```bash
# Add user to video group
sudo usermod -aG video $USER

# Logout and login again, or use:
newgrp video

# Or run with sudo (not recommended)
sudo python3 capture_y10.py
```

**Problem:** Device not found
```bash
# List available video devices
v4l2-ctl --list-devices

# Check if camera is detected
ls -la /dev/video*

# Check if driver is loaded
lsmod | grep vc_mipi
```

### Format Issues

**Problem:** "Format mismatch" warning
```bash
# Set correct format before capture
v4l2-ctl -d /dev/video0 --set-fmt-video=pixelformat=Y10

# Verify
v4l2-ctl -d /dev/video0 --get-fmt-video
```

**Problem:** Unsupported format
```bash
# List supported formats
v4l2-ctl -d /dev/video0 --list-formats-ext

# Some cameras may not support all formats
# Check camera documentation for supported formats
```

### Capture Issues

**Problem:** "No space left on device" when saving frames
```bash
# Check disk space
df -h

# Reduce frame count or use different directory
python3 capture_y10.py --count 10 --save --output-dir /path/to/storage
```

**Problem:** Script hangs or no frames captured
```bash
# Check if another process is using the camera
lsof /dev/video0

# Kill blocking process
sudo fuser -k /dev/video0

# Restart camera driver
sudo rmmod vc_mipi_core
sudo modprobe vc_mipi_core
```## Processing Captured Frames

### Loading and Analyzing Y10 Frames

```python
import numpy as np
import matplotlib.pyplot as plt

# Load Y10 frame (16-bit)
frame = np.load('frames_y10/frame_0001.npy')

# Extract 10-bit values
frame_10bit = frame >> 6

# Statistics
print(f"Resolution: {frame.shape}")
print(f"Data type: {frame.dtype}")
print(f"16-bit range: {frame.min()}-{frame.max()}")
print(f"10-bit range: {frame_10bit.min()}-{frame_10bit.max()}")
print(f"Mean (10-bit): {frame_10bit.mean():.1f}")

# Histogram
plt.hist(frame_10bit.ravel(), bins=256, range=(0, 1023))
plt.title('10-bit Histogram')
plt.xlabel('Pixel Value (10-bit)')
plt.ylabel('Count')
plt.show()

# Convert to 8-bit for visualization
frame_8bit = (frame >> 8).astype(np.uint8)
plt.imshow(frame_8bit, cmap='gray')
plt.title('Y10 Frame (8-bit display)')
plt.show()
```

### Loading and Analyzing GREY Frames

```python
import numpy as np
import matplotlib.pyplot as plt

# Load GREY frame (8-bit)
frame = np.load('frames_grey/frame_0001.npy')

# Statistics
print(f"Resolution: {frame.shape}")
print(f"Data type: {frame.dtype}")
print(f"Range: {frame.min()}-{frame.max()}")
print(f"Mean: {frame.mean():.1f}")

# Histogram
plt.hist(frame.ravel(), bins=256, range=(0, 255))
plt.title('8-bit Histogram')
plt.xlabel('Pixel Value')
plt.ylabel('Count')
plt.show()

# Display
plt.imshow(frame, cmap='gray', vmin=0, vmax=255)
plt.title('GREY Frame')
plt.colorbar()
plt.show()
```

### Batch Processing Multiple Frames

```python
import numpy as np
from pathlib import Path

# Load all frames
frame_dir = Path('frames_y10')
frame_files = sorted(frame_dir.glob('frame_*.npy'))

print(f"Processing {len(frame_files)} frames...")

# Calculate temporal statistics
frames = []
for f in frame_files:
    frame = np.load(f)
    frame_10bit = frame >> 6
    frames.append(frame_10bit)

frames = np.array(frames)

# Temporal mean and std
mean_frame = frames.mean(axis=0)
std_frame = frames.std(axis=0)

print(f"Temporal mean range: {mean_frame.min():.1f}-{mean_frame.max():.1f}")
print(f"Temporal std range: {std_frame.min():.1f}-{std_frame.max():.1f}")

# Save results
np.save('temporal_mean.npy', mean_frame)
np.save('temporal_std.npy', std_frame)
```

## Performance Tips

1. **Use virtual environment** - Isolate dependencies and avoid system conflicts
2. **Pre-configure format** - Use v4l2-ctl before running scripts for consistent behavior
3. **Choose appropriate format** - Use GREY for standard applications, Y10 for high dynamic range
4. **Buffer count** - Scripts use 4 buffers by default (adjustable in code)
5. **Save selectively** - Only save frames when needed; .npy files are large
6. **Process in batches** - Load multiple frames into memory for efficient batch processing

## File Formats

### .npy Format (NumPy arrays)
Both scripts save frames as NumPy `.npy` files:
- **Advantages:** Fast, preserves exact data types, no compression artifacts
- **Disadvantages:** Large file size, not viewable in standard image viewers
- **Loading:** `frame = np.load('frame_0001.npy')`

### Converting to Standard Formats

```python
import numpy as np
from PIL import Image

# Load Y10 frame
frame = np.load('frame_0001.npy')
frame_8bit = (frame >> 8).astype(np.uint8)

# Save as PNG
img = Image.fromarray(frame_8bit)
img.save('frame_0001.png')

# Or use matplotlib
import matplotlib.pyplot as plt
plt.imsave('frame_0001.png', frame_8bit, cmap='gray')
```

## Further Reading

- [V4L2 Documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/v4l2.html)
- [VC MIPI Driver Documentation](../docs/)
- [Pixel Format Details](../docs/pixelformat.md)
- [libcamera Integration](../docs/libcamera.md)
- [Build from Source](../docs/build_from_source.md)

## Support

For issues and questions:
- Check [FAQ](../docs/faq.md)
- Review [manual installation guide](../docs/manual_installation.md)
- Open an issue on GitHub
