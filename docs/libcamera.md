# Libcamera support 
Libcamera support is specifically for ISP use. 
The ISP uses optimisations like
- automatic white balance (awb)
- colour correction matrix (ccm)

# Installation

Copy the Makefile from repo to your device
*or* the following steps have to done manually
You can also download file directy by:
```shell
wget https://raw.githubusercontent.com/VC-MIPI-modules/vc_mipi_raspi/main/Makefile
```
## Raspberry Pi 4
Call ```make all-rpi4``` 
## Raspberry Pi 5
Call ```make all``` 

# Manual steps
## 1.Install dependencies
```shell
sudo apt-get update
sudo apt install -y python3-pip git python3-jinja2 python3-yaml python3-ply
sudo apt install -y libboost-dev libsdl2-dev libc6 libevent-dev
sudo apt install -y libgnutls28-dev openssl libtiff-dev pybind11-dev
sudo apt install -y libtiff-dev qt6-base-dev qt6-tools-dev-tools
sudo apt install -y meson cmake
sudo apt install -y libglib2.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install -y  libboost-program-options-dev libdrm-dev libexif-dev
sudo apt install -y libpisp-dev
```

## 2.Install forked version of libcamera

```shell 
git clone --branch update-to-v0.7.0 https://github.com/VC-MIPI-modules/libcamera
cd libcamera
meson setup build --buildtype=release -Dpipelines=rpi/vc4,rpi/pisp -Dipas=rpi/vc4,rpi/pisp -Dv4l2=true -Dgstreamer=enabled -Dtest=false -Dlc-compliance=disabled -Dcam=enabled -Dqcam=disabled -Ddocumentation=disabled -Dpycamera=enabled --prefix=/usr
sudo ninja -C build install
```

## 3.Install rpi cam apps
```shell
sudo apt install -y rpicam-apps
```

## 4.Enable libcamera support for vc mipi

1. Run ```vc-config```
2. Enable support for each port individually
3. Path ```Configure Device Tree > cam0/1 > Lanes Configuration > Manufacturer Selection > Libcamera Support ```
4. Reboot system

# Known sensor-specific notes

## IMX900 — ISP line length warning
When using the IMX900, libcamera prints the following error on startup:

```
ERROR IPARPI ipa_base.cpp:589 Sensor minimum line length of 4.55us (449.99 MPix/s) is below the minimum allowable ISP limit of 5.39us (380 MPix/s)
ERROR IPARPI ipa_base.cpp:595 THIS WILL CAUSE IMAGE CORRUPTION!!! Please update the camera sensor driver to allow more horizontal blanking control.
```

This is **expected behavior** for the IMX900. The sensor's HMAX register is fixed and cannot be changed, so the horizontal blanking is reported as read-only. The ISP warning can be safely ignored — images are not corrupted in practice.

## IMX412 — 2-lane mode required
The IMX412 must be configured to run in **2-lane mode** when used with libcamera.

## Raspberry Pi 5 — H.264 encoding
The Raspberry Pi 5 has no hardware H.264 encoder. When running `rpicam-vid`, the default H.264 codec will fail with:
```
ERROR: *** Unable to find an appropriate H.264 codec ***
```
Use software encoding via libav instead:
```shell
rpicam-vid -t 10000 --codec libav --libav-video-codec h264 -o output.mp4
```
This requires rpicam-apps to be built with `-Denable_libav=enabled` (included in the build command above) and the `libavcodec-dev` packages installed.

# Adjustments for support
1. The exposure values are not in mikroseconds anymore. 
2. The raw export of the images on ```/dev/video0``` and ```/dev/video5``` are only possible before the start of libcamera
3. If the raw export is needed again, run ```set_rpi5_pipeline```

# Examples on usage
```shell
gst-launch-1.0 libcamerasrc  !      video/x-raw,colorimetry=bt709,format=NV12,width=1280,height=1080,framerate=10/1 !      queue ! jpegenc ! multipartmux !      tcpserversink host=0.0.0.0 port=5000
```
On the host system, install Gstreamer and connect by this to raspi5 (Works on Windows, Linux)
```
gst-launch-1.0 tcpclientsrc host=<IP OR RASPI> port=5000 ! multipartdemux ! jpegdec ! autovideosink 
```
# Disclaimer
The support is still in beta version and can contain some issues. 
The images are color corrected, but a calibration with the used lenses may be needed.
