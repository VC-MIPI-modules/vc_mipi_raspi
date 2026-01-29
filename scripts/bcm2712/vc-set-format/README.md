# vc-set-format

A lightweight C utility for setting V4L2 video format parameters with proper validation and error handling.

## Features

- Direct ioctl-based V4L2 format configuration
- Format validation before setting
- Automatic fallback for older kernel versions (with/without colorspace parameter)
- Format verification after setting
- Comprehensive error reporting with exit codes
- Syslog integration for logging
- List supported formats

## Building

```bash
make
```

To build with debug symbols:
```bash
make debug
```

## Installation

```bash
sudo make install
```

This installs the binary to `/usr/local/bin/vc-set-format`.

## Usage

```bash
vc-set-format [options] <device> <width> <height> <fourcc> [colorspace]
```

### Arguments

- `device`: Video device path (e.g., `/dev/video0`)
- `width`: Video width in pixels
- `height`: Video height in pixels
- `fourcc`: FourCC pixel format code (e.g., `pRAA`, `RG16`, `Y10P`)
- `colorspace`: Optional colorspace (default: `srgb`)

### Options

- `-v, --verbose`: Enable verbose output
- `-l, --list`: List supported formats for a device
- `-h, --help`: Show help message

### Examples

Set format to pRAA (SRGGB10) at 1920x1080:
```bash
vc-set-format /dev/video0 1920 1080 pRAA
```

Set format with explicit colorspace:
```bash
vc-set-format /dev/video0 1920 1080 RG16 srgb
```

List supported formats:
```bash
vc-set-format -l /dev/video0
```

Verbose output:
```bash
vc-set-format -v /dev/video0 640 480 Y10P
```

### Exit Codes

- `0`: Success
- `1`: Format not supported by device
- `2`: Failed to set format
- `3`: Invalid arguments
- `4`: Device not found or cannot open

## Integration with vc-config

This utility can be called from shell scripts to replace complex v4l2-ctl commands:

```bash
# Instead of:
v4l2-ctl -d "$videodev" --set-fmt-video=width=$width,height=$height,pixelformat="$fmt",colorspace=srgb

# Use:
vc-set-format "$videodev" "$width" "$height" "$fmt" srgb
```

The utility automatically handles:
- Format support checking
- Colorspace parameter fallback for older kernels
- Format verification
- Detailed error reporting

## Cleanup

```bash
make clean
```

## Uninstallation

```bash
sudo make uninstall
```
