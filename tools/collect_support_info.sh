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
# Device tree

section "Device Tree Export"
if command -v dtc &> /dev/null; then
    dtc -I fs /sys/firmware/devicetree/base -O dts -o "$OUTDIR/export.dts" >> "$INFO" 2>&1 && \
        echo "Device tree exported to export.dts" >> "$INFO" || \
        echo "Failed to export device tree" >> "$INFO"
else
    echo "dtc not available" >> "$INFO"
fi

section "I2C Bus Scan"
if command -v i2cdetect &> /dev/null; then
    for i2cbus in /dev/i2c-*; do
        if [ -e "$i2cbus" ]; then
            busnum=$(basename "$i2cbus" | sed 's/i2c-//')
            echo "" >> "$INFO"
            echo "--- Bus $busnum ($i2cbus) ---" >> "$INFO"
            i2cdetect -y "$busnum" >> "$INFO" 2>&1
        fi
    done
else
    echo "i2cdetect not available (install i2c-tools)" >> "$INFO"
fi

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
# Camera diagnostic capture
section "Camera Diagnostic Capture"
echo "Checking for VC MIPI cameras..." >> "$INFO"

camera_found=false
for mediadev in /dev/media*; do
    if [ -e "$mediadev" ] && media-ctl -d "$mediadev" -p 2>/dev/null | grep -q "vc_mipi_camera"; then
        camera_found=true
        
        # Extract subdevice and video device
        subdev=$(media-ctl -d "$mediadev" -p 2>/dev/null | grep "vc_mipi_camera" -A 2 | grep "device node name" | awk '{print $4}')
        videodev=$(media-ctl -d "$mediadev" -p 2>/dev/null | grep "rp1-cfe-csi2_ch0\|unicam-image" -A 2 | grep "device node name" | awk '{print $4}' | head -n 1)
        
        if [ -n "$subdev" ] && [ -n "$videodev" ] && [ -e "$subdev" ] && [ -e "$videodev" ]; then
            echo "" >> "$INFO"
            echo "--- Camera: $subdev -> $videodev ---" >> "$INFO"
            
            # Save current debug level
            if [ -f /sys/module/vc_mipi_core/parameters/debug ]; then
                current_debug=$(cat /sys/module/vc_mipi_core/parameters/debug 2>/dev/null)
                
                # Set debug level to 6 (registers)
                echo "Setting debug level to 6..." >> "$INFO"
                echo 6 | sudo tee /sys/module/vc_mipi_core/parameters/debug > /dev/null 2>&1
            fi
            
            # Set trigger_mode to 0 (self-trigger/continuous)
            echo "Setting trigger_mode=0..." >> "$INFO"
            v4l2-ctl --device="$subdev" --set-ctrl=trigger_mode=0 >> "$INFO" 2>&1
            
            # Capture a few frames with timeout (5 seconds)
            V4L2LOG="$OUTDIR/v4l2_capture_$(basename $videodev).log"
            echo "Capturing frames (5 second timeout)..." >> "$INFO"
            {
                echo "--- Video format ---"
                v4l2-ctl --device="$videodev" --get-fmt-video 2>&1
                echo ""
                echo "--- Video device controls ---"
                v4l2-ctl --device="$videodev" --list-ctrls-menus 2>&1
                echo ""
                echo "--- Subdevice controls ($subdev) ---"
                v4l2-ctl --device="$subdev" --list-ctrls-menus 2>&1
                echo ""
                echo "--- Stream capture (10 frames, verbose) ---"
                timeout 5 v4l2-ctl --device="$videodev" --stream-mmap --stream-count=10 --verbose 2>&1 \
                    || echo "Frame capture completed (timeout or count reached)"
            } | tee -a "$INFO" >> "$V4L2LOG"
            
            # Dump dmesg for this camera to a separate file
            echo "Dumping kernel messages..." >> "$INFO"
            sudo dmesg | tail -n 200 > "$OUTDIR/dmesg_camera_$(basename $videodev).log"
            
            # Restore previous debug level
            if [ -n "$current_debug" ]; then
                echo "$current_debug" | sudo tee /sys/module/vc_mipi_core/parameters/debug > /dev/null 2>&1
            fi
        fi
    fi
done

if [ "$camera_found" = false ]; then
    echo "No VC MIPI cameras detected" >> "$INFO"
fi

# ---------------------------------------------------------------------------
# Logs (kept as separate files)
echo "Collecting logs..."
sudo dmesg > "$OUTDIR/dmesg.log"
sudo journalctl -k -n 500 > "$OUTDIR/journalctl_kernel.log"

echo ""
echo "Support package created in: $OUTDIR"
echo "  system_info.txt  - all system/hardware/driver information"
echo "  export.dts       - device tree export"
echo "  dmesg.log        - full kernel ring buffer"
echo "  journalctl_kernel.log - recent kernel journal entries"
echo "  v4l2_capture_*.log - v4l2-ctl stream output from camera diagnostic capture
  dmesg_camera_*.log - kernel messages from camera diagnostic capture"
echo ""

# Create tar.gz archive
ARCHIVE="$HOME/$(basename $OUTDIR).tar.gz"
echo "Creating archive..."
if (cd "$HOME" && tar -czf "$(basename $OUTDIR).tar.gz" "$(basename $OUTDIR)"); then
    echo "✓ Archive created successfully: $ARCHIVE"
    echo ""
    echo "Please attach this file to your support ticket."
else
    echo "✗ Failed to create archive automatically."
    echo "Please create it manually with:"
    echo "  cd \$HOME && tar -czf $(basename $OUTDIR).tar.gz $(basename $OUTDIR)"
fi