# Libcamera support 
Libcamera support is specifically for ISP use. 
The ISP uses optimisations like
- automatic white balance (awb)
- colour correction matrix (ccm)

# Installation

Copy the Makefile from repo to your device and call ```make all```
*or* the following steps have to done manually
You can also download file directy by:
```shell
wget https://raw.githubusercontent.com/VC-MIPI-modules/vc_mipi_raspi/main/Makefile
```
## Raspberry Pi 4

Call ```make all-rpi4``` instead of make all

## 1.Install dependencies
```shell
sudo apt-get update
sudo apt-get install meson
sudo apt install -y python3-pip git python3-jinja2
sudo apt install -y libboost-dev libsdl2-dev libc6 libevent-dev
sudo apt install -y libgnutls28-dev openssl libtiff-dev pybind11-dev
sudo apt install -y libtiff-dev qt6-base-dev qt6-tools-dev-tools
sudo apt install -y meson cmake
sudo apt install -y python3-yaml python3-ply
sudo apt install -y libglib2.0-dev libgstreamer-plugins-base1.0-dev
sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev
git clone https://github.com/VC-MIPI-modules/libpisp
cd libpisp
meson setup build
meson compile -C build
sudo meson install -C build
```

## 2.Install forked version of libcamera

```shell 
git clone https://github.com/VC-MIPI-modules/libcamera
cd libcamera
meson setup build --buildtype=release -Dpipelines=rpi/vc4,rpi/pisp -Dipas=rpi/vc4,rpi/pisp -Dv4l2=true -Dgstreamer=enabled -Dtest=false -Dlc-compliance=disabled -Dcam=enabled -Dqcam=disabled -Ddocumentation=disabled -Dpycamera=enabled --prefix=/usr
sudo ninja -C build install
```

## 3.Install rpi cam apps
```shell
git clone https://github.com/raspberrypi/rpicam-apps.git
cd rpicam-apps
git checkout v1.5.2
meson setup build
meson compile -C build
sudo meson install -C build
echo "/usr/local/lib/aarch64-linux-gnu" > /etc/ld.so.conf.d/rpicam.conf
sudo ldconfig
```

## 4.Enable libcamera support for vc mipi

1. Run ```vc-config```
2. Enable support for each port individually
3. Path ```Configure Device Tree > cam0/1 > Lanes Configuration > Manufacturer Selection > Libcamera Support ```
4. Reboot system

# Adjustments for support
1. The exposure values are not in mikroseconds anymore. 
2. The raw export of the images on ```/dev/video0``` and ```/dev/video5``` are only possible before the start of libcamera
3. If the raw export is needed again, run ```set_rpi5_pipeline```

# Examples on usage
```shell
gst-launch-1.0 libcamerasrc  !      video/x-raw,colorimetry=bt709,format=NV12,width=1280,height=1080,framerate=10/1 !      queue ! jpegenc ! multipartmux !      tcpserversink host=0.0.0.0 port=5000
```

# Disclaimer
The support is still in beta version and can contain some issues. 
The images are color corrected, but a calibration with the used lenses may be needed.
