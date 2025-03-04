# Frame rate
```shell
v4l2-ctl -c frame_rate=<value> -d <subdevice>
#Example 10 fps(Hz) for first camera
v4l2-ctl -c frame_rate=10000 -d /dev/v4l-subdev2
```
The range is from 0 to maximum from the sensor and the configuration. 

The max frame rate can be seen at the start of system in ```dmesg```.

The frame rate depends on: 
* sensor type
* lanes configuration
* Bit depth
* ROI
* Streaming mode 
* Exposure time

The value's unit is mHz.

If set to 0, the maximum frame rate will be set at the start of streaming.
The maximum of the control will also be set at the start of streaming.
If values are set that reduce the frame rate, the frame rate will be reduced automatically, 
even though the frame rate is set to a value. 
This means that a long exposure time will reduce the maximum frame rate: 
An exposure time of 100 msec / 0.1 sec means a maximum frame rate of 1/(0.1 sec) = 10 Hz.

