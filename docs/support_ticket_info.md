# Support Ticket Information

When submitting a support ticket, please provide the following information to help us resolve your issue quickly.
For a quick summary, you can run the export script under the next section or do step by step later in the docs described.
> [!TIP]
> When sharing logs, please ensure they don't contain sensitive information. You can redact IP addresses, hostnames, or credentials if present.

## Complete Support Package

You can create a complete support package by running:

```bash
#!/bin/bash
# Create support info bundle

OUTDIR="$HOME/vc_mipi_support_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$OUTDIR"

INFO="$OUTDIR/system_info.txt"

# Helper: append a titled section to system_info.txt
section() {
    echo "" >> "$INFO"
    echo "################################################################################" >> "$INFO"
    echo "## $1" >> "$INFO"
    echo "################################################################################" >> "$INFO"
}

echo "Collecting system information..."

# ---------------------------------------------------------------------------
# Hardware info
section "Raspberry Pi Model"
tr -d '\0' < /proc/device-tree/model >> "$INFO" 2>/dev/null && echo "" >> "$INFO"

section "CPU Info"
cat /proc/cpuinfo >> "$INFO"

section "vcgencmd version"
vcgencmd version >> "$INFO" 2>/dev/null

# ---------------------------------------------------------------------------
# OS info
section "OS Release"
cat /etc/os-release >> "$INFO"

section "Kernel Version (uname -a)"
uname -a >> "$INFO"

section "Architecture"
dpkg --print-architecture >> "$INFO"

# ---------------------------------------------------------------------------
# Boot config
section "Boot Config (/boot/firmware/config.txt or /boot/config.txt)"
cat /boot/firmware/config.txt >> "$INFO" 2>/dev/null || cat /boot/config.txt >> "$INFO" 2>/dev/null

section "Boot cmdline"
cat /boot/firmware/cmdline.txt >> "$INFO" 2>/dev/null || cat /boot/cmdline.txt >> "$INFO" 2>/dev/null

section "Active dtoverlay/dtparam entries (uncommented)"
grep -v '^#' /boot/firmware/config.txt 2>/dev/null | grep -E 'dtoverlay|dtparam' >> "$INFO" \
    || grep -v '^#' /boot/config.txt 2>/dev/null | grep -E 'dtoverlay|dtparam' >> "$INFO"

section "Installed VC MIPI Overlays"
ls -la /boot/firmware/overlays/vc-mipi* >> "$INFO" 2>/dev/null || ls -la /boot/overlays/vc-mipi* >> "$INFO" 2>/dev/null

section "config.txt Include Files"
CFG=/boot/firmware/config.txt; [[ -f $CFG ]] || CFG=/boot/config.txt
DIR=$(dirname "$CFG")
grep '^include' "$CFG" >> "$INFO" 2>/dev/null
grep '^include' "$CFG" 2>/dev/null | awk '{print $2}' | while read f; do
    echo "" >> "$INFO"
    echo "--- $f ---" >> "$INFO"
    cat "$DIR/$f" >> "$INFO" 2>/dev/null || echo "(not found)" >> "$INFO"
done

# ---------------------------------------------------------------------------
# Driver info
section "Installed VC MIPI Packages (dpkg)"
dpkg -l | grep vc-mipi >> "$INFO"

section "DKMS Status"
dkms status >> "$INFO"

section "Loaded VC MIPI Kernel Modules"
lsmod | grep vc_mipi >> "$INFO"

section "Module Info: vc_mipi_core"
modinfo vc_mipi_core >> "$INFO" 2>&1

section "Module Info: vc_mipi_camera"
modinfo vc_mipi_camera >> "$INFO" 2>&1

# ---------------------------------------------------------------------------
# V4L2 / media info
section "V4L2 Devices"
v4l2-ctl --list-devices >> "$INFO" 2>&1

section "V4L2 Formats (/dev/video0)"
v4l2-ctl --list-formats-ext -d /dev/video0 >> "$INFO" 2>&1

section "V4L2 Controls (/dev/video0)"
v4l2-ctl --list-ctrls-menus -d /dev/video0 >> "$INFO" 2>&1

section "Media Controller Topology (all /dev/media* devices)"
for dev in /dev/media*; do
    echo "" >> "$INFO"
    echo "=== $dev ===" >> "$INFO"
    media-ctl -d "$dev" --print-topology >> "$INFO" 2>&1
done

# ---------------------------------------------------------------------------
# Logs (kept as separate files)
echo "Collecting logs..."
sudo dmesg > "$OUTDIR/dmesg.log"
sudo journalctl -k -n 500 > "$OUTDIR/journalctl_kernel.log"

echo ""
echo "Support package created in: $OUTDIR"
echo "  system_info.txt  - all system/hardware/driver information"
echo "  dmesg.log        - full kernel ring buffer"
echo "  journalctl_kernel.log - recent kernel journal entries"
echo ""
echo "Please zip this folder and attach it to your support ticket:"
echo "  cd \$HOME && tar -czf $(basename $OUTDIR).tar.gz $(basename $OUTDIR)"
```

Save this script, make it executable, and run it:

```bash
chmod +x collect_support_info.sh
./collect_support_info.sh
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

Check if camera is detected on I2C bus:

```bash
sudo i2cdetect -y 10
```

(Bus number may vary: typically 0, 1, 10, or 11 depending on CSI port and Pi model)

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

