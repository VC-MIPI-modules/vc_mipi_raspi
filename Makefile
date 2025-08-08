.PHONY: all installDeps installLibPisp installLibcamera installRPICamApps installLibcamera-rpi4 installGstreamer installRPICamAppsHailo
all: installDeps installLibPisp installGstreamer installLibcamera  installRPICamApps
all-rpi4: installDeps installGstreamer installLibcamera-rpi4  installRPICamApps
installDeps:
	@echo "Installing dependencies..."
	## 1.Install dependencies
	sudo apt-get update
	sudo apt-get -y install meson
	sudo apt install -y python3-pip git python3-jinja2
	sudo apt install -y libboost-dev libsdl2-dev libc6 libevent-dev
	sudo apt install -y libgnutls28-dev openssl libtiff-dev pybind11-dev
	sudo apt install -y libtiff-dev qt6-base-dev qt6-tools-dev-tools
	sudo apt install -y meson cmake
	sudo apt install -y python3-yaml python3-ply
	sudo apt install -y libglib2.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
	sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev libpng-dev

installLibPisp:
	cd /tmp && rm -rf /tmp/libpisp && \
	git clone https://github.com/VC-MIPI-modules/libpisp || true && \
	cd libpisp && \
	meson setup build && \
	meson compile -C build && \
	sudo meson install -C build

installLibcamera:
	sudo apt-get remove -y libcamera* || true
	cd /tmp && rm -rf /tmp/libcamera && \
	git clone https://github.com/VC-MIPI-modules/libcamera || true && \
	cd libcamera && \
	meson setup build --buildtype=release --prefix=/usr \
	  -Dpipelines=rpi/vc4,rpi/pisp \
	  -Dipas=rpi/vc4,rpi/pisp \
	  -Dv4l2=true \
	  -Dgstreamer=enabled \
	  -Dtest=false \
	  -Dlc-compliance=disabled \
	  -Dcam=enabled \
	  -Dqcam=disabled \
	  -Ddocumentation=disabled \
	  -Dpycamera=enabled && \
	meson compile -C build && \
	sudo ninja -C build install
installLibcamera-rpi4:
	sudo apt-get remove -y libcamera* || true
	cd /tmp && rm -rf /tmp/libcamera && \
	git clone https://github.com/VC-MIPI-modules/libcamera || true && \
	cd libcamera && \
	meson setup build --buildtype=release --prefix=/usr \
	  -Dpipelines=rpi/vc4 \
	  -Dipas=rpi/vc4 \
	  -Dv4l2=true \
	  -Dgstreamer=enabled \
	  -Dtest=false \
	  -Dlc-compliance=disabled \
	  -Dcam=enabled \
	  -Dqcam=disabled \
	  -Ddocumentation=disabled \
	  -Dpycamera=enabled && \
	meson compile -C build && \
	sudo ninja -C build install
installGstreamer:
	sudo apt install -y gstreamer1.0-tools gstreamer1.0-plugins-base libgstreamer-plugins-base1.0-dev \
	gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
	gstreamer1.0-libav
installRPICamApps:
	cd /tmp && rm -rf /tmp/rpicam-apps && \
	git clone https://github.com/raspberrypi/rpicam-apps.git && \
	cd rpicam-apps && \
	git checkout v1.5.2 && \
	meson setup build  --buildtype=release --prefix=/usr -Ddownload_hailo_models=false -Ddownload_imx500_models=false -Denable_imx500=false && \
	meson compile -C build && \
	sudo meson install -C build
	@printf "%s\n" \
	 "/usr/local/lib/aarch64-linux-gnu" \
	 "/usr/lib/aarch64-linux-gnu" \
	| sudo tee /etc/ld.so.conf.d/rpicam.conf	&& \
	sudo ldconfig

installRPICamAppsHailo:
	sudo apt install hailo-tappas-core=3.30.0-1 hailo-dkms=4.19.0-1 hailort=4.19.0-3 libepoxy-dev
	cd /tmp && rm -rf /tmp/rpicam-apps && \
	git clone https://github.com/raspberrypi/rpicam-apps.git && \
	cd rpicam-apps && \
	git fetch --tags && \
	git checkout v1.5.2 && \
	meson setup build  --buildtype=release -Denable_hailo=enabled -Denable_opencv=enabled -Ddownload_hailo_models=true -Denable_egl=enabled -Denable_imx500=false && \
	meson compile -C build && \
	sudo meson install -C build 
	@printf "%s\n" \
	 "/usr/local/lib/aarch64-linux-gnu" \
	 "/usr/lib/aarch64-linux-gnu" \
	| sudo tee /etc/ld.so.conf.d/rpicam.conf	&& \
	sudo ldconfig
	sudo cp -r /usr/local/share/hailo-models /usr/share/
