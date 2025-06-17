DTC ?= dtc
DTC_FLAGS ?= -@ -I dts -O dtb -Wno-unit_address_vs_reg

SENSOR_NAMES := vc-mipi-bcm2837-cam0
SENSOR_NAMES += vc-mipi-bcm2837-cam1
SENSOR_NAMES += vc-mipi-common-memory-contiguous
SENSOR_NAMES += vc-mipi-bcm2835-vccmi10-fpgaio
SENSOR_NAMES += vc-mipi-bcm2835-vccmi10-plcio
SENSOR_NAMES += vc-mipi-bcm2835-vccmi10-sd1
SENSOR_NAMES += vc-mipi-bcm2835-vccmi10-spi0
SENSOR_NAMES += vc-mipi-common-memory-contiguous
DEVICETREES	:= $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
DEVICETREES_RM := $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
all: $(DEVICETREES) dtbo
dtbo: $(DEVICETREES)
	@echo "\n### DTBOs are built ###\n"


%.dtbo: %-overlay.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<

install: $(DEVICETREES)
	@echo "\n### Installing DTBOs ###\n"
	install -D -m 644 $(DEVICETREES) /boot/firmware/overlays/