.PHONY: all installDeps installLibPisp installLibcamera installLibcameraApps
all: installDeps installLibPisp installLibcamera installLibcameraApps

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
	sudo apt install -y libglib2.0-dev libgstreamer-plugins-base1.0-dev
	sudo apt install -y cmake libboost-program-options-dev libdrm-dev libexif-dev

installLibPisp:
	cd /tmp && rm -rf /tmp/libpisp && \
	git clone https://github.com/raspberrypi/libpisp || true && \
	cd libpisp && \
	meson setup build && \
	meson compile -C build && \
	sudo meson install -C build

installLibcamera:
	cd /tmp && rm -rf /tmp/libcamera && \
	git clone https://github.com/VC-MIPI-modules/libcamera || true && \
	cd libcamera && \
	meson setup build --buildtype=release \
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

installLibcameraApps:
	cd /tmp && rm -rf /tmp/rpicam-apps && \
	git clone https://github.com/raspberrypi/rpicam-apps.git && \
	cd rpicam-apps && \
	meson setup build && \
	meson compile -C build && \
	sudo meson install -C build
	echo "/usr/local/lib/aarch64-linux-gnu" > /etc/ld.so.conf.d/rpicam.conf
	sudo ldconfig

