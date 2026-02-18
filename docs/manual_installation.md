# Download installation files
With git:
```shell
sudo apt-get update && sudo apt-get install git
git clone https://github.com/VC-MIPI-modules/vc_mipi_raspi.git
```
Without git:
```shell
wget https://github.com/VC-MIPI-modules/vc_mipi_raspi/archive/refs/heads/main.zip
unzip vc_mipi_raspi-main.zip
```

# Installation

## Install drivers

```
cd src
make build install reload
```

## Install scripts

Choose the right version for your platform

* Raspi5, CM5    ⇒  BCM2712 
* Raspi4, CM4    ⇒  BCM2711
* Raspi3+        ⇒  BCM2837
* Raspi Zero 2   ⇒  RP3A0  
```
cd scripts/XXXX
make installScripts
```

## Install overlays

```
cd overlays/overlays-XXXX
make install
```

# Test installation

1. Reboot system
2. Check if sensor is detected 
   In vc-config the sensors should be displayed
3. Check if the sensor is streaming by "Set ROI" in the vc-config tool