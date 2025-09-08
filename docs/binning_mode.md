# Binning mode

With the following v4l2 control the binning mode of the sensor can be set
```shell
v4l2-ctl -c binning_mode=<index>
```
The value determines the mode and depends on the capabilities of the sensor. There are predefined value pairs for the horizontal and the vertical binning factors, which are addressed by an index. <br>
E.g. IMX412:
```shell
v4l2-ctl -c binning_mode=1
```
means horizontal binning is 1, vertical binning is 2 (1x2)
The predefined value pairs can be seen in the vc_init_ctrl_xxx function of the sensor. In that case vc_init_ctrl_imx412(...)<br>
When setting a binning mode, the ROI must be adjusted accordingly.


The table below should give an overview about the maximal resolutions of the sensor's binning modes:

| sensor module | binning_mode <br> index | binning factors <br>(horizontal x vertical) | maximal resolution <br> (width x height) | comment |
| ------------- | ------------------ | --------------- | ----------- | ----------------- |
| IMX273        | 0                  |      0 x 0      | 1440 x 1080 | no binning at all |
|               | 1                  |      2 x 2      |  720 x  540 | 2 pixels horizontal <br> 2 pixels vertical |
| IMX296        | 0                  |      0 x 0      | 1440 x 1080 | no binning at all |
|               | 1                  |      2 x 2      |  720 x  540 | 2 pixels horizontal <br> 2 pixels vertical |
| IMX412        | 0                  |      0 x 0      | 4032 x 3040 | no binning at all |
|               | 1                  |      1 x 2      | 4032 x 1520 | 2 pixels vertical |
|               | 2                  |      2 x 2      | 2016 x 1520 | 2 pixels horizontal <br> 2 pixels vertical |
|               | 3                  |      4 x 2      |  992 x 1520 | 4 pixels horizontal <br> 2 pixels vertical |
|               | 4                  |      8 x 2      |  480 x 1520 | 8 pixels horizontal <br> 2 pixels vertical |
|               | 5                  |     16 x 2      |  224 x 1520 | 16 pixels horizontal <br> 2 pixels vertical <br>  extended binning |
| IMX565        | 0                  |      0 x 0      | 4128 x 3000 | no binning at all |
|               | 1                  |      2 x 2      | 2048 x 1504 | 2 pixels horizontal <br> 2 pixels vertical |
| IMX566        | 0                  |      0 x 0      | 2848 x 2848 | no binning at all |
|               | 1                  |      2 x 2      | 1408 x 1408 | 2 pixels horizontal <br> 2 pixels vertical |
| IMX567/568    | 0                  |      0 x 0      | 2464 x 2064 | no binning at all |
|               | 1                  |      2 x 2      | 1216 x 1032 | 2 pixels horizontal <br> 2 pixels vertical |


It is also possible to set a ROI in combination with the binning modes. The width must be a multiple of 32 pixels and the height must be a multiple of 8. Both values must be less than the maximal values given in the table above. Please see also **[ROI cropping](ROI_CROPPING.md)**

When binning with Pregius S (IMX56x), the sensor is getting monochrome.

## Examples
### V4L
```shell
$ v4l2-ctl -d <SUBDEV> --set-fmt-video=width=4032,height=3040 -c binning_mode=0
```
> IMX412 without binning.

<br>

```shell
$ v4l2-ctl -d <SUBDEV> --set-fmt-video=width=2016,height=1520 -c binning_mode=2
```
> IMX412 with 2x2 binning.

<br>

For setting the offset, refer to [Roi Position](./roi_position.md)
