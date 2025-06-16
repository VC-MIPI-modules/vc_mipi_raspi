
# Manual Camera Configuration Steps
If you do not use the *vc-config* script, you can configure your VC MIPI camera manually using the following commands:
## 1. List Media Devices and Entities
First, identify your media device and subdevice nodes:
```shell
media-ctl -p -d /dev/media0
```
Example:
```
- entity 16: vc_mipi_camera 11-001a (1 pad, 1 link)
             type V4L2 subdev subtype Sensor flags 0
             device node name /dev/v4l-subdev2
        pad0: Source
                [fmt:Y8_1X8/640x480 field:none colorspace:srgb
                 crop.bounds:(0,0)/1440x1080
                 crop:(100,100)/640x480
                 compose.bounds:(0,0)/0x0
                 compose:(0,0)/0x0]
                -> "csi2":0 [ENABLED,IMMUTABLE]
```
Look for your camera sensor (e.g., vc_mipi_camera) and note the subdevice node (e.g., v4l-subdev2) and the video node (e.g., video0).
## 2. Query Supported Formats
List the supported media bus formats for your subdevice:
```shell
v4l2-ctl --device=/dev/v4l-subdev2 --get-subdev-fmt pad=0
```
| Code    | FourCC | Description                          |
|---------|--------|--------------------------------------|
| 0x3001  | BA81   | MEDIA_BUS_FMT_SBGGR8_1X8             |
| 0x3002  | GRBG   | MEDIA_BUS_FMT_SGRBG8_1X8             |
| 0x3013  | GBRG   | MEDIA_BUS_FMT_SGBRG8_1X8             |
| 0x3014  | RGGB   | MEDIA_BUS_FMT_SRGGB8_1X8             |
| 0x2001  | GREY   | MEDIA_BUS_FMT_Y8_1X8                 |
| 0x300e  | pGAA   | MEDIA_BUS_FMT_SGBRG10_1X10           |
| 0x300f  | pRAA   | MEDIA_BUS_FMT_SRGGB10_1X10           |
| 0x200a  | Y10P   | MEDIA_BUS_FMT_Y10_1X10 (Y10P on Raspi)|
| 0x300a  | pgAA   | MEDIA_BUS_FMT_SGRBG10_1X10           |
| 0x3007  | pBAA   | MEDIA_BUS_FMT_SBGGR10_1X10           |
| 0x3008  | pBCC   | MEDIA_BUS_FMT_SBGGR12_1X12           |
| 0x3010  | pGCC   | MEDIA_BUS_FMT_SGBRG12_1X12           |
| 0x3011  | pgCC   | MEDIA_BUS_FMT_SGRBG12_1X12           |
| 0x3012  | pRCC   | MEDIA_BUS_FMT_SRGGB12_1X12           |
| 0x2013  | Y12P   | MEDIA_BUS_FMT_Y12_1X12               |
| 0x3019  | pBEE   | MEDIA_BUS_FMT_SBGGR14_1X14           |
| 0x301a  | pGEE   | MEDIA_BUS_FMT_SGBRG14_1X14           |
| 0x301b  | pgEE   | MEDIA_BUS_FMT_SGRBG14_1X14           |
| 0x301c  | pREE   | MEDIA_BUS_FMT_SRGGB14_1X14           |
| 0x202d  | Y14P   | MEDIA_BUS_FMT_Y14_1X14               |
| 0x301d  | BYR2   | MEDIA_BUS_FMT_SBGGR16_1X16           |
| 0x301e  | GB16   | MEDIA_BUS_FMT_SGBRG16_1X16           |
| 0x301f  | GR16   | MEDIA_BUS_FMT_SGRBG16_1X16           |
| 0x3020  | RG16   | MEDIA_BUS_FMT_SRGGB16_1X16           |
| 0x202e  | Y16    | MEDIA_BUS_FMT_Y16_1X16               |

## 3.  Set the Sensor Format
Set the desired format and resolution for the sensor entity. Replace ENTITY, FMT, WIDTH, and HEIGHT with your values:
```shell
media-ctl -d /dev/media0 -V "'ENTITY':0 [fmt:FMT/WIDTHxHEIGHT field:none colorspace:srgb]"
```
Example:
```shell
media-ctl -d /dev/media0 -V "'vc_mipi_camera 10-001a':0 [fmt:SRGGB12_1X12/1920x1080 field:none colorspace:srgb]"
```
## 4.  Set Cropping or ROI (Region of Interest)
To set cropping (offset and size):
```shell
media-ctl -d /dev/media0 --set-v4l2 "'ENTITY':0[crop:(LEFT,TOP)/WIDTHxHEIGHT]"
```
Example:
```shell
media-ctl -d /dev/media0 --set-v4l2 "'vc_mipi_camera 10-001a':0[crop:(100,100)/640x480]"
```
## 5. Set the Video Node Format
Set the format for the video node (e.g., video0):
```shell
v4l2-ctl -d /dev/video0 --set-fmt-video=width=WIDTH,height=HEIGHT,pixelformat=FMT,colorspace=srgb
```
Example:
```shell
v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=RG12,colorspace=srgb
```
## 6.  Test Streaming
Test if the camera is streaming correctly:
```shell
v4l2-ctl --verbose --stream-mmap --device=/dev/video0 --stream-count=3
```
