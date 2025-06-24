# Frame rate
```shell
v4l2-ctl -c analogue_gain=<value> -d <subdevice>
#Example 10.000 dB  for first camera
v4l2-ctl -c analogue_gain=10000 -d /dev/v4l-subdev2
```
The range is from 0 to the sensor's maximum value.
Check the maximum value with v4l2-ctl.
```shell
v4l2-ctl --all -d /dev/v4l-subdev2
```
The value's unit is mdB.

