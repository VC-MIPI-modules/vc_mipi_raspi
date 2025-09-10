# Pixel Format

Modern image sensors support a variety of pixel formats, each optimized for different applications. Typically, sensors output data in their native packed format, maximizing bandwidth and efficiency.

On the Raspberry Pi 5, the MIPI receiver can automatically unpack sensor data, making it easier and faster to access pixel values as a 2D array. For example, 10-bit and 12-bit sensor formats are expanded to 16 bits per pixel, with values shifted accordingly:

- **10-bit format:** Value range is 0–1023  
  After conversion: `value << 6` (shifted left by 6 bits)  
  New range: 0–65535

- **12-bit format:** Value range is 0–4095  
  After conversion: `value << 4` (shifted left by 4 bits)  
  New range: 0–65535

This unpacking simplifies image processing and ensures compatibility with standard software tools.

## Example Calculation

For a 10-bit sensor value:
```
Original: 1023 (maximum 10-bit value: 2¹⁰ - 1)
After conversion: 1023 << 6 = 65472
```

## Changing Pixel Format

You can change the pixel format using the `vc-config` utility. This tool configures the media pads to select the desired format, allowing you to work with either packed or unpacked data as needed.

For more details on supported formats, see the official [Linux Pixel Formats documentation](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/pixfmt-bayer.html).

## Color Sensor Formats (Bayer Pattern)

Color sensors deliver raw data in a Bayer pattern, where each pixel captures only one color component. The pattern determines how Red (R), Green (G), and Blue (B) pixels are arranged on the sensor.

### Common Bayer Patterns

Most sensors use the **RGGB** format, while some specialized sensors like the IMX226 and IMX415 use **GBRG**.

```
RGGB Pattern:                    GBRG Pattern:
┌─────┬─────┬─────┬─────┐       ┌─────┬─────┬─────┬─────┐
│  R  │  G  │  R  │  G  │       │  G  │  B  │  G  │  B  │
├─────┼─────┼─────┼─────┤       ├─────┼─────┼─────┼─────┤
│  G  │  B  │  G  │  B  │       │  R  │  G  │  R  │  G  │
├─────┼─────┼─────┼─────┤       ├─────┼─────┼─────┼─────┤
│  R  │  G  │  R  │  G  │       │  G  │  B  │  G  │  B  │
├─────┼─────┼─────┼─────┤       ├─────┼─────┼─────┼─────┤
│  G  │  B  │  G  │  B  │       │  R  │  G  │  R  │  G  │
└─────┴─────┴─────┴─────┘       └─────┴─────┴─────┴─────┘
```

### Pattern Breakdown

| Format | Top-Left 2×2 Block | Description |
|--------|-------------------|-------------|
| **RGGB** | `R G`<br>`G B` | Red starts at position (0,0) |
| **GBRG** | `G B`<br>`R G` | Green starts at position (0,0) |

> **Note:** The Bayer pattern repeats every 2×2 pixels. Notice that Green appears twice as often as Red or Blue, matching the human eye's sensitivity to green light.


