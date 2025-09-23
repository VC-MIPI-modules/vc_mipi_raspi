DTC ?= dtc
DTC_FLAGS ?= -@ -I dts -O dtb -Wno-unit_address_vs_reg

SENSOR_NAMES := vc-mipi-rp3a0-cam0
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