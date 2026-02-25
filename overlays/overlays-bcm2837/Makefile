DTC ?= dtc
DTC_FLAGS ?= -@ -I dts -O dtb -Wno-unit_address_vs_reg

SENSOR_NAMES := vc-mipi-bcm2837-cam0
SENSOR_NAMES += vc-mipi-bcm2837-cam1
SENSOR_NAMES += vc-mipi-common-memory-contiguous
DEVICETREES	:= $(foreach sentype,$(SENSOR_NAMES),$(sentype).dtbo)
all: $(DEVICETREES) dtbo
dtbo: $(DEVICETREES)
	@echo "\n### DTBOs are built ###\n"


%.dtbo: %-overlay.dts
	$(DTC) $(DTC_FLAGS) -o $@ $<

install: $(DEVICETREES)
	@echo "\n### Installing DTBOs ###\n"
	sudo cp $(DEVICETREES)  /boot/firmware/overlays/
	sudo cp config_vc-mipi-driver-bcm2837.txt /boot/firmware/
	sudo chmod 644 /boot/firmware/overlays/$(DEVICETREES)
	sudo chmod 644 /boot/firmware/config_vc-mipi-driver-bcm2837.txt
	@echo "\n### DTBOs are installed ###\n"