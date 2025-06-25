################################################################################
# Makefile
#
# Mipi Module Sensor Driver Vision Components GmbH Ettlingen 2019-2022
#
# Author: Michael Steinel, MBE (Martin Engel)
#
################################################################################
# Source .env if present and export its keys
ifneq ("$(wildcard .env)","")
include .env
export $(shell sed -n 's/=.*//' .env)
endif

EXTRA_CFLAGS += -Wno-missing-prototypes


SENSOR_NAMES := vc-mipi-bcm2711-cam0
SENSOR_NAMES += vc-mipi-bcm2711-cam1
SENSOR_NAMES += vc-mipi-common-memory-contiguous


################################################################################

UBUNTU_CONFIG		= /boot/firmware/config.txt
UBUNTU_LAYOUT		= $(shell [ -f $(UBUNTU_CONFIG) ] && echo 1 || echo 0 )
ifeq ($(UBUNTU_LAYOUT), 1)
BOOT_OVERLAYS_DIR	= /boot/firmware/overlays
else
BOOT_OVERLAYS_DIR	= /boot/overlays
endif

KERNEL_I2C_DIR := /lib/modules/$(shell uname -r)/kernel/drivers/media/i2c/

SENSORDRIVERS := vc_mipi_core/vc_mipi_core.ko
SENSORDRIVERS += vc_mipi_core/vc_mipi_modules.ko
SENSORDRIVERS += vc_mipi_camera/vc_mipi_camera.ko
SENSOR_DRIVER := vc_mipi_core
SENSOR_DRIVER += vc_mipi_modules
SENSOR_DRIVER += vc_mipi_camera
DEVICETREES	:= $(foreach sentype,$(SENSOR_NAMES),overlays/$(sentype).dtbo)
DEVICETREES_RM := $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
OVERLAY_DIRS := $(wildcard ../overlays/overlays-*)
obj-m := vc_mipi_core/
obj-m += vc_mipi_camera/

.PHONY: all copy-overlays


dtbo: 
	@for d in $(OVERLAY_DIRS); do \
		echo ">>> building DTBOs in $$d"; \
		$(MAKE) -C $$d all; \
	done

dtbo_current: 
	$(MAKE) -C overlays all

KERNEL_HEADERS ?= /lib/modules/$(shell uname -r)/build
# Debug information
$(info KERNEL_HEADERS=$(KERNEL_HEADERS))
$(info Checking for required files in $(KERNEL_HEADERS)...)

# Check for required files
REQUIRED_FILES := scripts/Makefile.build scripts/basic/Makefile include/config/auto.conf.cmd
MISSING_FILES := $(foreach file,$(REQUIRED_FILES),$(if $(wildcard $(KERNEL_HEADERS)/$(file)),,$(file)))

ifeq ($(MISSING_FILES),)
$(info All required files are present.)
else
endif

ifeq ($(wildcard $(KERNEL_HEADERS)),)
$(error Kernel headers not found. Please install them with 'sudo apt-get install raspberrypi-kernel-headers')
endif

build:
	EXTRA_CFLAGS=$(EXTRA_CFLAGS) $(MAKE) -C $(KERNEL_HEADERS) M=$(PWD) modules

all: build

clean:
	$(MAKE) -C $(KERNEL_HEADERS) M=$(PWD) clean
	-rm -f $(SENSORDRIVERS)
	-rm -f $(DEVICETREES)

reload:
	@LOGGING_LEVEL=$$(cat /sys/module/vc_mipi_core/parameters/debug 2>/dev/null || echo 3); \
	sudo modprobe -r bcm2835-unicam; \
	sudo modprobe -r rp1_cfe; \
	sudo modprobe -r vc_mipi_camera; \
	sudo modprobe -r vc_mipi_modules; \
	sudo modprobe -r vc_mipi_core; \
	sudo modprobe bcm2835-unicam; \
	sudo modprobe rp1_cfe verbose_debug=1; \
	sudo modprobe vc_mipi_core; \
	sudo modprobe vc_mipi_modules; \
	sudo modprobe vc_mipi_camera; \
	echo $$LOGGING_LEVEL | sudo tee /sys/module/vc_mipi_core/parameters/debug >/dev/null
unload:
	-sudo modprobe -r vc_mipi_camera
	-sudo modprobe -r vc_mipi_modules
	-sudo modprobe -r vc_mipi_core


dev:	clean all install reload

installScripts:
	sudo mkdir -p /usr/local/bin
	sudo cp set_rpi5_pipeline /usr/local/bin
	sudo chmod +x /usr/local/bin/set_rpi5_pipeline
	sudo cp 99-camera-setup.rules /etc/udev/rules.d/
	sudo cp vc-config /usr/local/bin/
	sudo chmod +x /usr/local/bin/vc-config
	grep -qxF "include config_vc-mipi-driver-bcm2712.txt" /boot/firmware/config.txt || echo "include config_vc-mipi-driver-bcm2712.txt" | sudo tee -a /boot/firmware/config.txt

install: all
	sudo install -p -m 644  $(SENSORDRIVERS)	/lib/modules/$(shell uname -r)/kernel/drivers/media/i2c/
	# sudo install -p -m 644  $(DEVICETREES)	 $(BOOT_OVERLAYS_DIR)/
	sudo /sbin/depmod -a $(shell uname -r)

uninstall:
	sudo rm -f $(foreach sencommon,$(SENSOR_DRIVER),$(KERNEL_I2C_DIR)/$(sencommon).ko)
	sudo rm -f $(foreach dtbo,$(DEVICETREES_RM),$(BOOT_OVERLAYS_DIR)/$(dtbo))
	sudo /sbin/depmod -a $(shell uname -r)


$(KERNEL_BUILD_DIR):
	@echo "--------------------------------------"
	@echo
	@echo "  Error:  Cannot continue due to the absence of the directory"
	@echo
	@echo "	$(KERNEL_BUILD_DIR)"
	@echo
	@echo "  You can try to fix it by installing the kernel header files package."
	@echo "  The following command may do so:"
	@echo
	@echo "	sudo  apt-get install  raspberrypi-kernel-headers"
	@echo
	@echo "--------------------------------------"
	@false


