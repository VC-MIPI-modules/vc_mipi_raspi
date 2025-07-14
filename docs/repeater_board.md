# VC MIPI Repeater Board Hardware Operating Manual

## 1. Hardware Compatibility
The VC MIPI Repeater Board is intended to be inserted into a VC MIPI signal link. It allows extending the cable length between the VC MIPI sensor and the host processor. It also makes it possible to access both hardware trigger input and flash output signals from the VC MIPI Module without soldering.

## 2. Technical Specification

| Component / Feature | Specification |
|---------------------|---------------|
| Number of lanes | 1–4 depending on sensor module |
| MIPI speed | max. 1.5Gbps |
| Flash output signal | 3.3V LVCMOS |
| Trigger input signal | 3.3V LVCMOS |
| Storage Conditions | Temperature: -20 to +60 deg C, Max. humidity: 90%, non condensing |
| Operating Conditions | Temperature: 0 to +50 deg C, Max. humidity: 80%, non condensing |
| Power Consumption | approx. 200mW @ 3.3V drawn from the host over the VC MIPI Cable |
| Sensor cable | 200 mm cable with 22 pins to sensor |
| Board to CPU cable | 200 mm cable with 15 pins or 22 pins if needed |


## 3. Jumper field

The board contains a jumper field with access to

the trigger input and flash output signals of the VC MIPI sensor, as well as
GND and 3.3V.
If the access to trigger and flash signals is not needed it is recommended to connect the signals flash from sensor and trigger to sensor of the sensor side VC MIPI cable with the host side VC MIPI cable using jumpers.

If you need to access to the signals, remove the jumpers and connect the signals with a 2mm MOLEX cable header with the jumper field. Make sure to use the GND pin as a reference level. If necessary it is possible to supply the 3.3V to your circuit from the VC MIPI Repeater Board.


## 4. Connecting the board

Connect the VC MIPI Repeater Board as shown in the figure Connecting the board:

To the left we have the 22 to 22 Pin Flexible Printed Circuit (FPC) Cable (EK003260). The cable needs to be showing the label CPU Side at this position. Further to the left the MIPI Camera module should be connected.

To the right we can either have the same 22 to 22 Pin FPC Cable or the 22 to 15 Pin FPC Cable (EK003261). At this position the cable needs to be showing the label MIPI Module Side. Further to the right the host CPU should be connected.