installScripts:
	sudo mkdir -p /usr/local/bin
	sudo cp set_rpi4_pipeline /usr/local/bin
	sudo chmod +x /usr/local/bin/set_rpi4_pipeline
	sudo cp 99-camera-setup.rules /etc/udev/rules.d/
	sudo cp vc-config /usr/local/bin/
	sudo chmod +x /usr/local/bin/vc-config
	grep -qxF "include config_vc-mipi-driver-bcm2711.txt" /boot/firmware/config.txt || echo "include config_vc-mipi-driver-bcm2711.txt" | sudo tee -a /boot/firmware/config.txt
