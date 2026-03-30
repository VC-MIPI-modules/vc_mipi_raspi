# Support Ticket Information

When submitting a support ticket, please provide the following information to help us resolve your issue quickly.
For a quick summary, you can run the export script under the next section or do step by step later in the docs described.
> [!TIP]
> When sharing logs, please ensure they don't contain sensitive information. You can redact IP addresses, hostnames, or credentials if present.

## Complete Support Package

You can create a complete support package by running the [collect_support_info.sh](../tools/collect_support_info.sh) script.

This script automatically collects:
- Hardware and OS information
- Boot configuration and device tree
- I2C bus scan
- Driver and package versions
- V4L2 device information
- Media controller topology
- Camera diagnostic capture with debug logging
- Kernel logs (dmesg and journalctl)

To run the script:

```bash
cd tools/
./collect_support_info.sh
```

The script will create a timestamped directory in your home folder with all the collected information. Compress and attach it to your support ticket:

```bash
cd $HOME
tar -czf vc_mipi_support_*.tar.gz vc_mipi_support_*/
```

## 1. Hardware Information

### Raspberry Pi Model and Variant

Run the following command to identify your Raspberry Pi model:

```bash
cat /proc/cpuinfo | grep -E "Model|Revision"
```

Or use:

```bash
tr -d '\0' < /proc/device-tree/model && echo
```

For more detailed hardware information:

```bash
vcgencmd version
```

### SoC/Chipset Information

Identify which BCM chip variant you have:

```bash
cat /proc/cpuinfo | grep "Hardware"
```

Common variants:
- **BCM2837** - Raspberry Pi 3
- **BCM2711** - Raspberry Pi 4
- **BCM2712** - Raspberry Pi 5
- **RP3A0** - Raspberry Pi Zero 2 W

## 2. Sensor Connection Details

Please provide the following information about your camera setup:

- **CSI Port used**: CSI-0, CSI-1 (if applicable)
- **Cable type**: Standard 15-pin, 22-pin, custom length
- **Repeater board**: Yes/No (model if applicable)
- **Camera module model**: (e.g., OV9281, IMX296, etc.)

### Check Connected Camera

```bash
vcgencmd get_camera
```

List all video devices:

```bash
v4l2-ctl --list-devices
```

## 3. Operating System Information

### Raspberry Pi OS Version

```bash
cat /etc/os-release
```

### Kernel Version

```bash
uname -a
```

### Architecture

```bash
dpkg --print-architecture
```

## 4. Boot Configuration

### Display Boot Config

```bash
cat /boot/firmware/config.txt
```

Or on older systems:

```bash
cat /boot/config.txt
```

### Check Active Device Tree Overlays

```bash
vcgencmd get_config int | grep dtoverlay
```

```bash
dtc -I fs /sys/firmware/devicetree/base | grep -A 10 "vc_mipi"
```

### Display cmdline.txt

```bash
cat /boot/firmware/cmdline.txt
```

Or:

```bash
cat /boot/cmdline.txt
```

### Installed VC MIPI Overlays

List all VC MIPI device tree overlay files installed in the boot partition:

```bash
ls -la /boot/firmware/overlays/vc-mipi*
```

Or on older systems:

```bash
ls -la /boot/overlays/vc-mipi*
```

Show the active dtoverlay entries from the boot config:

```bash
grep -v '^#' /boot/firmware/config.txt | grep -E 'dtoverlay|dtparam'
```

Or on older systems:

```bash
grep -v '^#' /boot/config.txt | grep -E 'dtoverlay|dtparam'
```

### Included Config Files

Raspberry Pi OS supports `include` directives in `config.txt`. Show which files are included and their contents:

```bash
CFG=/boot/firmware/config.txt
[[ -f $CFG ]] || CFG=/boot/config.txt
grep '^include' "$CFG"
```

Display the content of each included file (e.g. `config_vc-mipi-driver-bcm2712.txt`):

```bash
CFG=/boot/firmware/config.txt
[[ -f $CFG ]] || CFG=/boot/config.txt
DIR=$(dirname "$CFG")
grep '^include' "$CFG" | awk '{print $2}' | while read f; do
    echo "=== $f ==="
    cat "$DIR/$f" 2>/dev/null || echo "(not found)"
done
```

## 5. Driver and Package Information

### Check Installed Driver Version

```bash
dpkg -l | grep vc-mipi
```

### List DKMS Modules

```bash
dkms status
```

### Check Loaded Kernel Modules

```bash
lsmod | grep vc_mipi
```

Get module information:

```bash
modinfo vc_mipi_core
modinfo vc_mipi_camera
```

## 6. Installation Logs (when installation fails)

### DKMS Installation Log

```bash
cat /var/lib/dkms/vc_mipi_*/*/build/make.log
```

Or for specific version (replace VERSION and KERNEL accordingly):

```bash
cat /var/lib/dkms/vc_mipi_core/VERSION/KERNEL/log/make.log
```

### Package Installation Log

```bash
cat /var/log/dpkg.log | grep vc-mipi
```

### APT History

```bash
cat /var/log/apt/history.log | grep -A 20 vc-mipi
```

### System Log During Installation

```bash
sudo journalctl -xe | grep -i vc_mipi
```

## 7. Runtime Logs and Diagnostics (when streaming fails)

### Kernel Ring Buffer (dmesg)

Capture the full dmesg output:

```bash
sudo dmesg | grep -i vc_mipi
```

Or save to file:

```bash
sudo dmesg > /tmp/dmesg_full.log
```

For real-time monitoring:

```bash
sudo dmesg -w
```

### System Logs

```bash
sudo journalctl -k | grep -i vc_mipi
```

Recent kernel messages:

```bash
sudo journalctl -k -n 200
```

### V4L2 Device Information

List video device capabilities:

```bash
v4l2-ctl --list-formats-ext -d /dev/video0
```

Get all controls:

```bash
v4l2-ctl --list-ctrls-menus -d /dev/video0
```

### Media Controller Information

List all available media devices:

```bash
ls /dev/media*
```

Print topology for each media device:

```bash
for dev in /dev/media*; do
    echo "=== $dev ==="
    media-ctl -d "$dev" --print-topology
done
```

### I2C Detection

The correct I2C bus number can be read directly from `dmesg`. The device identifier before the colon shows `<bus>-<address>`, so `10-001a` means the camera is expected on **bus 10** at address `0x1a`:

```
[    5.313824] vc_mipi_camera 10-001a: vc_mod_setup(): Unable to get module I2C client for address 0x10
[    5.313853] vc_mipi_camera 10-001a: probe with driver vc_mipi_camera failed with error -5
```

Use that bus number with `i2cdetect` to check if the sensor is physically reachable:

```bash
sudo i2cdetect -y 10
```

(Bus number may vary: typically 0, 1, 10, or 11 depending on CSI port and Pi model. Always check `dmesg` first to confirm the expected bus.)

## 8. Test Streaming Attempt

Try to capture a frame and save the error output:

```bash
v4l2-ctl --device /dev/video0 --stream-mmap --stream-count=1 2>&1 | tee /tmp/stream_error.log
```

Or with gstreamer:

```bash
gst-launch-1.0 v4l2src device=/dev/video0 num-buffers=10 ! fakesink 2>&1 | tee /tmp/gst_error.log
```


## 10. Additional Information

Please also include:

- **Description of the issue**: What is not working? What did you expect to happen?
- **Steps to reproduce**: What exact commands or steps lead to the problem?
- **When did it start**: Did it work before? What changed?
- **Error messages**: Any specific error messages you received
- **Workarounds attempted**: What have you already tried?

---

