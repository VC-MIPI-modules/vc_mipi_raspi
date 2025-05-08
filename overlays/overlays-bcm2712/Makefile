DTC ?= dtc
DTC_FLAGS ?= -@ -I dts -O dtb -Wno-unit_address_vs_reg

SENSOR_NAMES := vc-mipi-bcm2712-cam0
SENSOR_NAMES += vc-mipi-bcm2712-cam1
SENSOR_NAMES += vc-mipi-common-memory-contiguous
DEVICETREES	:= $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
DEVICETREES_RM := $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
all: $(DEVICETREES) dtbo
dtbo: $(DEVICETREES)
	@echo "\n### DTBOs are built ###\n"


%.dtbo: %-overlay.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<
