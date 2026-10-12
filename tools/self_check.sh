#!/bin/bash
# Self check for the VC MIPI driver installation and camera configuration.
#
# Checks the installed package, the driver build and modules, the boot
# configuration (lanes, sensor manufacturer, libcamera), the active device
# tree (camera node, CSI receiver link), the devices on the camera I2C bus,
# the module ROM, whether the driver probed the connected camera(s) and,
# optionally, whether frames arrive.
#
# Nothing is changed on the system. The sensor module is only read via I2C.
#
# Usage: self_check.sh [--no-stream]
#   --no-stream   skip the short streaming test

STREAM_TEST=true
for arg in "$@"; do
    case "$arg" in
        --no-stream) STREAM_TEST=false ;;
        -h|--help)   sed -n '2,/^[^#]/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 0 ;;
        *)           echo "Unknown option: $arg"; exit 2 ;;
    esac
done

if [ -t 1 ]; then
    C_OK=$'\e[32m'; C_WARN=$'\e[33m'; C_FAIL=$'\e[31m'; C_INFO=$'\e[36m'; C_BOLD=$'\e[1m'; C_OFF=$'\e[0m'
fi

n_ok=0; n_warn=0; n_fail=0

ok()    { echo "  ${C_OK}[ OK ]${C_OFF} $1"; n_ok=$((n_ok+1)); }
warn()  { echo "  ${C_WARN}[WARN]${C_OFF} $1"; n_warn=$((n_warn+1)); }
fail()  { echo "  ${C_FAIL}[FAIL]${C_OFF} $1"; n_fail=$((n_fail+1)); }
info()  { echo "  ${C_INFO}[INFO]${C_OFF} $1"; }
hint()  { echo "         ⇒ $1"; }
title() { echo ""; echo "${C_BOLD}$1${C_OFF}"; }

# Kernel log since boot. The journal keeps the probe messages even when the
# ring buffer has already been overwritten (e.g. by gain messages).
klog() {
    journalctl -k -b -o cat --no-pager 2>/dev/null || dmesg 2>/dev/null || sudo -n dmesg 2>/dev/null
}
KLOG=$(klog)

# ---------------------------------------------------------------------------
# Platform
title "Platform"

MODEL=$(tr -d '\0' < /proc/device-tree/model 2>/dev/null)
# Maximum lanes per VC camera port (cam0 cam1), 0 = connector not available.
# On BCM2711/BCM2837 the VC cam0 overlay uses csi1 (up to 4 lanes),
# cam1 uses csi0 (2 lanes).
case "$MODEL" in
    *"Raspberry Pi 500"*)         SOC=bcm2712; MAX_LANES=(0 0) ;;
    *"Raspberry Pi 5"*)           SOC=bcm2712; MAX_LANES=(4 4) ;;
    *"Compute Module 5"*)         SOC=bcm2712; MAX_LANES=(4 4) ;;
    *"Raspberry Pi 400"*)         SOC=bcm2711; MAX_LANES=(0 0) ;;
    *"Compute Module 4"*)         SOC=bcm2711; MAX_LANES=(4 2) ;;
    *"Raspberry Pi 4"*)           SOC=bcm2711; MAX_LANES=(2 0) ;;
    *"Compute Module 3"*)         SOC=bcm2837; MAX_LANES=(2 2) ;;
    *"Raspberry Pi 3"*)           SOC=bcm2837; MAX_LANES=(2 0) ;;
    *"Zero 2"*)                   SOC=rp3a0;   MAX_LANES=(2 0) ;;
    *)                            SOC="";      MAX_LANES=("" "") ;;
esac

if [ -n "$SOC" ]; then
    ok "$MODEL ($SOC)"
else
    warn "Unknown model: '$MODEL' - lane limits of the connectors are not checked"
fi
info "Kernel $(uname -r)"

# ---------------------------------------------------------------------------
# Package and driver
title "Driver installation"

PKG=$(dpkg-query -W -f='${Package} ${Status}\n' 'vc-mipi-driver-*' 2>/dev/null \
      | awk '$4 == "installed" {print $1}' | head -n 1)
PKG_SUFFIX=${PKG#vc-mipi-driver-}
if [ -z "$PKG" ]; then
    fail "No vc-mipi-driver package installed"
    hint "sudo apt install ./vc-mipi-driver-${SOC:-<soc>}_<version>_arm64.deb"
else
    PKG_VERSION=$(dpkg-query -W -f='${Version}' "$PKG")
    if [ -n "$SOC" ] && [ "$PKG_SUFFIX" != "$SOC" ] && [ "$PKG_SUFFIX" != "vccmi10" ]; then
        fail "Package $PKG $PKG_VERSION does not fit this board (expected vc-mipi-driver-$SOC)"
        hint "Remove it and install vc-mipi-driver-$SOC"
    else
        ok "Package $PKG $PKG_VERSION"
    fi
fi

if command -v dkms &>/dev/null; then
    if dkms status 2>/dev/null | grep -q "vc-mipi-driver.*$(uname -r).*installed"; then
        ok "DKMS module built for running kernel"
    else
        fail "DKMS module not installed for kernel $(uname -r)"
        hint "sudo dkms autoinstall, then reboot (linux headers for this kernel needed)"
    fi
else
    warn "dkms not found"
fi

modules_ok=true
for m in vc_mipi_core vc_mipi_modules vc_mipi_camera; do
    if [ ! -d /sys/module/$m ]; then
        fail "Kernel module $m not loaded"
        modules_ok=false
    fi
done
if ! $modules_ok; then
    if ! modinfo vc_mipi_camera &>/dev/null; then
        hint "vc_mipi_camera.ko is not installed for kernel $(uname -r), see DKMS above"
    else
        load_err=$(echo "$KLOG" | grep -iE 'vc_mipi.*(unknown symbol|disagrees|invalid module|exec format|key was rejected|required key)' | tail -n 3)
        if [ -n "$load_err" ]; then
            echo "$load_err" | sed 's/^/         | /'
            hint "The module failed to load, rebuild it: sudo dkms autoinstall"
        else
            hint "The module is loaded automatically for a VC camera node in the device tree (see cameras below)"
            hint "Load it manually to see errors: sudo modprobe vc_mipi_camera && dmesg | tail"
        fi
    fi
fi
if $modules_ok; then
    MOD_VERSION=$(cat /sys/module/vc_mipi_camera/version 2>/dev/null)
    ok "Kernel modules loaded (version $MOD_VERSION)"
    if [ -n "$PKG_VERSION" ] && [ -n "$MOD_VERSION" ] && [ "${PKG_VERSION%%-*}" != "$MOD_VERSION" ]; then
        warn "Loaded module version $MOD_VERSION differs from package version $PKG_VERSION"
        hint "Reboot to load the installed version"
    fi
fi

# ---------------------------------------------------------------------------
# Boot configuration
title "Boot configuration"

CFG=/boot/firmware/config.txt; [ -f $CFG ] || CFG=/boot/config.txt
BOOT_DIR=$(dirname "$CFG")
VC_CFG_NAME="config_vc-mipi-driver-${PKG_SUFFIX:-$SOC}.txt"
VC_CFG="$BOOT_DIR/$VC_CFG_NAME"

if [ ! -f "$VC_CFG" ]; then
    fail "$VC_CFG not found"
elif ! grep -q "^include $VC_CFG_NAME" "$CFG"; then
    fail "$CFG does not include $VC_CFG_NAME"
    hint "Add the line 'include $VC_CFG_NAME' to $CFG"
else
    ok "$CFG includes $VC_CFG_NAME"
fi

# Active (uncommented) lines of the VC configuration
CFG_LINES=$(grep -E '^[[:space:]]*dt(overlay|param)=' "$VC_CFG" 2>/dev/null | sed 's/[[:space:]#].*$//')

# ---------------------------------------------------------------------------
# Cameras

# I2C helpers. The module does not auto-increment reliably on multi-byte
# reads, so read byte by byte.
i2c_rb() {   # bus addr reg -> decimal value
    local v
    v=$(i2ctransfer -f -y "$1" w2@"$2" $(printf '0x%02x 0x%02x' $(($3 >> 8)) $(($3 & 0xff))) r1 2>/dev/null) || return 1
    echo $((v))
}
i2c_rstr() { # bus addr reg len -> string up to the first NUL
    local s="" i c
    for ((i = 0; i < $4; i++)); do
        c=$(i2c_rb "$1" "$2" $(($3 + i))) || return 1
        [ "$c" -eq 0 ] && break
        s+=$(printf "\\$(printf '%03o' "$c")")
    done
    echo "$s"
}

MOD_ADDR=0x10           # VC module controller
DESC_BASE=0x1000        # module descriptor (struct vc_desc)
MOD_REG_SEN_ADDR=0x0105 # sensor I2C address the module is wired for

manu_name() {
    case "$1" in
        26) echo sony ;;  # 0x1a
        96) echo ov ;;    # 0x60
        54) echo os ;;    # 0x36
        *)  printf '0x%02x' "$1" ;;
    esac
}
manu_addr() {
    case "$1" in
        ov) echo 96 ;;
        os) echo 54 ;;
        *)  echo 26 ;;
    esac
}

# Device tree node status, a missing status property means "okay"
dt_status() {
    local s
    s=$(tr -d '\0' < "$1/status" 2>/dev/null)
    echo "${s:-okay}"
}

# State of an address in 'i2cdetect -y -r' output: present, bound (UU) or absent
i2c_scan_state() { # scan addr
    local row cells cell
    row=$(echo "$1" | grep "^$(printf '%02x' $(($2 & 0xf0))):")
    cells=${row#*:}
    cell=${cells:$((3 * ($2 & 0x0f) + 1)):2}
    case "$cell" in
        UU)            echo bound ;;
        [0-9a-f][0-9a-f]) echo present ;;
        *)             echo absent ;;
    esac
}

LIBCAMERA_LIST=""
if command -v rpicam-hello &>/dev/null; then
    LIBCAMERA_LIST=$(rpicam-hello --list-cameras 2>/dev/null)
fi

DT_BASE=$(readlink -f /proc/device-tree)
cameras_found=0

for idx in 0 1; do
    cam="cam$idx"
    title "Camera $cam"

    # --- configuration file ---------------------------------------------------
    overlay=$(echo "$CFG_LINES" | grep -E "^dtoverlay=vc-mipi-.*-$cam$" | head -n 1)
    cfg_lanes=$(echo "$CFG_LINES" | sed -n "s/^dtparam=${cam}_lanes\([0-9]\)$/\1/p" | tail -n 1)
    cfg_manu=$(echo "$CFG_LINES" | sed -n "s/^dtparam=${cam}_manu_\([a-z]*\)$/\1/p" | tail -n 1)
    cfg_libcam=$(echo "$CFG_LINES" | sed -n "s/^dtparam=${cam}_libcamera_\([a-z]*\)$/\1/p" | tail -n 1)
    max=${MAX_LANES[$idx]}

    if [ -z "$overlay" ] || [ "$cfg_lanes" = "0" ]; then
        info "Disabled in $VC_CFG_NAME"
        continue
    fi
    info "Config: ${overlay#dtoverlay=}, lanes=${cfg_lanes:-?}, manufacturer=${cfg_manu:-sony (default)}, libcamera=${cfg_libcam:-off}"

    dtbo="$BOOT_DIR/overlays/${overlay#dtoverlay=}.dtbo"
    [ -f "$dtbo" ] || fail "Overlay $dtbo not found"

    if [ "$max" = "0" ]; then
        warn "This board has no connector for $cam"
        hint "Comment out '$overlay' in $VC_CFG to avoid a failing probe"
        continue
    fi
    for p in lanes manu_ libcamera_; do
        if echo "$CFG_LINES" | grep -qx "dtparam=${cam}_$p"; then
            fail "Incomplete line 'dtparam=${cam}_$p' (value missing) in $VC_CFG"
        fi
    done
    if [ -z "$cfg_lanes" ]; then
        fail "No dtparam=${cam}_lanesN set"
        hint "Add dtparam=${cam}_lanes2 (or 1/4, see table in $VC_CFG_NAME) after '$overlay'"
    elif [ -n "$max" ] && [ "$cfg_lanes" -gt "$max" ]; then
        fail "${cam}_lanes$cfg_lanes configured, but the $cam connector of this board has only $max lanes"
        hint "Set dtparam=${cam}_lanes$max in $VC_CFG"
    fi

    # --- active device tree (what the kernel booted with) ---------------------
    node=""
    for c in $(grep -l -a "vc,vc_mipi_camera" $(find "$DT_BASE" -path "*/camera$idx/compatible" 2>/dev/null) 2>/dev/null); do
        node=$(dirname "$c"); break
    done
    if [ -z "$node" ]; then
        fail "No camera$idx node (compatible vc,vc_mipi_camera) in the active device tree"
        if [ -f "$dtbo" ]; then
            hint "'$overlay' was not applied: reboot after installing or changing $VC_CFG_NAME"
        else
            hint "Reinstall the package, the overlay file is missing"
        fi
        continue
    fi
    dt_rel=${node#$DT_BASE}
    dt_lanes=$(( $(stat -c %s "$node/port/endpoint/data-lanes" 2>/dev/null || echo 0) / 4 ))
    dt_addr=$(( 0x$(od -An -tx1 -j3 -N1 "$node/reg" | tr -d ' ') ))
    dt_libcam=off; [ -e "$node/libcamera" ] && dt_libcam=on

    # CSI receiver at the other end of the endpoint link
    csi_node=""; csi_lanes=0
    rp=$(od -An -tx1 "$node/port/endpoint/remote-endpoint" 2>/dev/null | tr -d ' \n')
    if [ -n "$rp" ]; then
        for p in $(find "$DT_BASE" -path '*/endpoint/phandle' 2>/dev/null); do
            if [ "$(od -An -tx1 "$p" | tr -d ' \n')" = "$rp" ]; then
                csi_ep=$(dirname "$p")
                csi_node=$(dirname "$(dirname "$csi_ep")")
                csi_lanes=$(( $(stat -c %s "$csi_ep/data-lanes" 2>/dev/null || echo 0) / 4 ))
                break
            fi
        done
    fi

    dt_ok=true
    if [ "$(dt_status "$node")" != "okay" ]; then
        fail "Device tree: $dt_rel is disabled (status=$(dt_status "$node"))"; dt_ok=false
    fi
    if [ "$(dt_status "$(dirname "$node")")" != "okay" ]; then
        fail "Device tree: I2C bus ${dt_rel%/*} is disabled"; dt_ok=false
    fi
    if [ "$dt_lanes" -eq 0 ]; then
        fail "Device tree: $dt_rel has no data-lanes"; dt_ok=false
    fi
    if [ -z "$csi_node" ]; then
        fail "Device tree: $dt_rel is not linked to a CSI receiver (remote-endpoint)"; dt_ok=false
    else
        csi_name="$(basename "$csi_node") ($(tr -d '\0' < "$csi_node/compatible" 2>/dev/null | cut -d, -f2))"
        if [ "$(dt_status "$csi_node")" != "okay" ]; then
            fail "Device tree: CSI receiver $csi_name is disabled"; dt_ok=false
        fi
        if [ "$csi_lanes" -ne "$dt_lanes" ]; then
            fail "Device tree: camera endpoint has $dt_lanes lanes, CSI endpoint $csi_lanes lanes"; dt_ok=false
            hint "Check the order in $VC_CFG_NAME: the dtparam=${cam}_... lines must follow '$overlay'"
        fi
    fi
    $dt_ok && ok "Device tree: $dt_rel @ $(printf '0x%02x' $dt_addr) → $csi_name, $dt_lanes lanes"

    if [ -n "$cfg_lanes" ] && [ "$dt_lanes" != "$cfg_lanes" ]; then
        warn "Active device tree uses $dt_lanes lanes, config says $cfg_lanes"
        hint "Reboot to apply the configuration"
    fi
    if [ "$dt_addr" != "$(manu_addr "${cfg_manu:-sony}")" ]; then
        warn "Active device tree uses sensor address $(printf '0x%02x' $dt_addr), config says ${cfg_manu:-sony}"
        hint "Reboot to apply the configuration"
    fi
    if [ "$dt_libcam" != "${cfg_libcam:-off}" ]; then
        warn "Active device tree has libcamera=$dt_libcam, config says ${cfg_libcam:-off}"
        hint "Reboot to apply the configuration"
    fi

    # --- I2C device and probe ------------------------------------------------
    i2cdev=""
    for d in /sys/bus/i2c/devices/*; do
        [ "$(readlink -f "$d/of_node" 2>/dev/null)" = "$node" ] && { i2cdev=$(basename "$d"); break; }
    done
    if [ -z "$i2cdev" ]; then
        fail "The kernel created no I2C device for $dt_rel"
        hint "The I2C bus ${dt_rel%/*} is not available"
        continue
    fi
    bus=${i2cdev%%-*}
    devlog=$(echo "$KLOG" | grep "vc_mipi_camera $i2cdev:")

    if [ "$(basename "$(readlink -f /sys/bus/i2c/devices/$i2cdev/driver)")" = "vc_mipi_camera" ]; then
        ok "Driver probed ($i2cdev, I2C bus $bus)"
        probed=true
        cameras_found=$((cameras_found+1))
    else
        probed=false
        fail "Driver not probed for $cam ($i2cdev)"
        if ! $modules_ok; then
            hint "The kernel module is not loaded, see 'Driver installation'"
        fi
        reason=$(echo "$devlog" | grep -iE 'unable|failed|not supported|mismatch|not detected|no communication' | tail -n 3)
        [ -n "$reason" ] && echo "$reason" | sed 's/^/         | /'
    fi

    # --- devices on the I2C bus ---------------------------------------------
    if ! command -v i2ctransfer &>/dev/null || ! command -v i2cdetect &>/dev/null; then
        warn "i2c-tools not installed, I2C and module checks skipped (sudo apt install i2c-tools)"
        $probed || continue
    elif [ ! -e /dev/i2c-$bus ]; then
        warn "/dev/i2c-$bus missing, I2C and module checks skipped (sudo modprobe i2c-dev)"
        $probed || continue
    elif [ ! -r /dev/i2c-$bus ] || [ ! -w /dev/i2c-$bus ]; then
        warn "No access to /dev/i2c-$bus, I2C and module checks skipped (run as root or add user to group i2c)"
        $probed || continue
    else
        scan=$(i2cdetect -y -r "$bus" 2>/dev/null)
        mod_state=$(i2c_scan_state "$scan" $((MOD_ADDR)))
        sen_state=$(i2c_scan_state "$scan" "$dt_addr")
        others=""
        for ((a = 0x03; a <= 0x77; a++)); do
            [ $a -eq $((MOD_ADDR)) ] || [ $a -eq "$dt_addr" ] && continue
            s=$(i2c_scan_state "$scan" $a)
            [ "$s" != "absent" ] && others+=" $(printf '0x%02x' $a)"
        done

        if [ "$mod_state" = "absent" ]; then
            fail "I2C bus $bus: no device at 0x10, no VC module answers"
            hint "Check that the module is connected to the $cam connector, the cable orientation and the power"
            [ -z "$others" ] && hint "If nothing is connected to $cam, disable it in $VC_CFG"
        elif [ "$mod_state" = "bound" ]; then
            drv=$(basename "$(readlink -f /sys/bus/i2c/devices/$bus-0010/driver)" 2>/dev/null)
            fail "I2C bus $bus: address 0x10 is used by the driver '${drv:-?}'"
            hint "Another camera driver claims the VC module address, e.g. via camera_auto_detect=1 in $CFG"
        else
            ok "I2C bus $bus: VC module answers at 0x10"
        fi
        case "$sen_state" in
            bound)   ok "I2C bus $bus: sensor at $(printf '0x%02x' $dt_addr) (in use by driver)" ;;
            present) ok "I2C bus $bus: sensor answers at $(printf '0x%02x' $dt_addr)" ;;
            absent)  if $probed; then
                         warn "I2C bus $bus: no sensor answers at $(printf '0x%02x' $dt_addr)"
                     else
                         info "I2C bus $bus: no sensor answers at $(printf '0x%02x' $dt_addr) (may be powered down after the failed probe)"
                     fi ;;
        esac
        [ -n "$others" ] && info "I2C bus $bus: other devices at$others"

        # --- module ROM (descriptor) ----------------------------------------
        if [ "$mod_state" = "present" ]; then
            if ! magic=$(i2c_rstr "$bus" $MOD_ADDR $DESC_BASE 12); then
                fail "VC module answers at 0x10, but reading the ROM failed"
            elif [ "$magic" != "mipi-module" ]; then
                fail "VC module ROM content is invalid (magic '$magic', expected 'mipi-module')"
                hint "Contact the support with the output of tools/collect_support_info.sh"
            else
                manuf=$(i2c_rstr "$bus" $MOD_ADDR $((DESC_BASE + 12)) 32)
                sen_manuf=$(i2c_rstr "$bus" $MOD_ADDR $((DESC_BASE + 46)) 8)
                sen_type=$(i2c_rstr "$bus" $MOD_ADDR $((DESC_BASE + 54)) 16)
                mod_id=$(( $(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 70))) + 256 * $(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 71))) ))
                mod_rev=$(( $(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 72))) + 256 * $(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 73))) ))
                num_modes=$(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 130)))
                [ "${num_modes:-0}" -gt 24 ] && num_modes=24
                supported=""
                for ((m = 0; m < ${num_modes:-0}; m++)); do
                    supported+="$(i2c_rb "$bus" $MOD_ADDR $((DESC_BASE + 134 + 16 * m + 4))) "
                done
                supported=$(echo $supported | tr ' ' '\n' | sort -nu | tr '\n' ' ')
                supported=${supported% }
                ok "Module ROM: $manuf, module ID $(printf '0x%04x' $mod_id) rev $mod_rev, sensor $sen_manuf $sen_type"
                info "Module ROM: $num_modes modes, supported lanes: ${supported// /, }"

                # Lanes
                best=$(for l in $supported; do [ -z "$max" ] || [ "$l" -le "$max" ] && echo $l; done | tail -n 1)
                if [ -n "$supported" ] && ! echo " $supported " | grep -q " $dt_lanes "; then
                    fail "$dt_lanes lanes active, but $sen_type supports only ${supported// /, } lanes"
                    [ -n "$best" ] && hint "Set dtparam=${cam}_lanes$best in $VC_CFG and reboot"
                else
                    ok "Lanes: $dt_lanes active, supported by $sen_type"
                    if [ -n "$best" ] && [ "$best" -gt "$dt_lanes" ]; then
                        info "$sen_type and this connector support $best lanes (higher frame rates): dtparam=${cam}_lanes$best"
                    fi
                fi

                # Sensor manufacturer (sets the sensor I2C address in the device tree)
                mod_sen_addr=$(i2c_rb "$bus" $MOD_ADDR $MOD_REG_SEN_ADDR)
                if [ -n "$mod_sen_addr" ] && [ "$mod_sen_addr" != "$dt_addr" ]; then
                    fail "Sensor manufacturer: module expects $(manu_name $mod_sen_addr), device tree is set to $(manu_name $dt_addr)"
                    hint "Set dtparam=${cam}_manu_$(manu_name $mod_sen_addr) in $VC_CFG and reboot"
                elif [ -n "$mod_sen_addr" ]; then
                    ok "Sensor manufacturer: $(manu_name $mod_sen_addr)"
                fi
            fi
        fi
    fi

    # Lane errors reported by the driver itself
    if echo "$devlog" | grep -q "Number of lanes .* not supported"; then
        fail "Driver: $(echo "$devlog" | grep -o 'Number of lanes .* not supported!' | tail -n 1)"
    fi

    $probed || continue

    # --- V4L2 nodes --------------------------------------------------------
    subdev=""; videodev=""; mediadev=""
    for md in /dev/media*; do
        topo=$(media-ctl -d "$md" -p 2>/dev/null)
        if echo "$topo" | grep -q "vc_mipi_camera $i2cdev"; then
            mediadev=$md
            subdev=$(echo "$topo" | grep "vc_mipi_camera $i2cdev" -A 3 | grep -o '/dev/v4l-subdev[0-9]*' | head -n 1)
            videodev=$(echo "$topo" | grep -E "rp1-cfe-csi2_ch0|unicam-image" -A 3 | grep -o '/dev/video[0-9]*' | head -n 1)
            break
        fi
    done
    if [ -n "$subdev" ] && [ -n "$videodev" ]; then
        ok "V4L2: $videodev, $subdev ($mediadev)"
    else
        fail "No media device / video node found for $i2cdev"
    fi

    # --- libcamera ------------------------------------------------------------
    libcam_idx=""
    if [ "$dt_libcam" = "on" ]; then
        case "$SOC" in
            bcm2712) tuning=/usr/share/libcamera/ipa/rpi/pisp/vc_mipi_camera.json ;;
            *)       tuning=/usr/share/libcamera/ipa/rpi/vc4/vc_mipi_camera.json ;;
        esac
        [ "$SOC" = "bcm2837" ] || [ "$SOC" = "rp3a0" ] && warn "libcamera support is only provided for Raspberry Pi 4 and 5"
        [ -f "$tuning" ] && ok "Tuning file $tuning" || fail "Tuning file $tuning missing (see docs/libcamera.md)"

        dt_path=/base${node#$DT_BASE}   # libcamera lists e.g. /base/soc/i2c0mux/i2c@1/camera0
        libcam_idx=$(echo "$LIBCAMERA_LIST" | grep "($dt_path)" | sed -n 's/^[[:space:]]*\([0-9]*\) :.*/\1/p')
        if [ -z "$LIBCAMERA_LIST" ] && ! command -v rpicam-hello &>/dev/null; then
            warn "rpicam-apps not installed, libcamera not checked"
        elif [ -n "$libcam_idx" ]; then
            ok "libcamera: camera $libcam_idx $(echo "$LIBCAMERA_LIST" | grep "($dt_path)" | sed 's/^[[:space:]]*[0-9]* : //; s/ (.*//')"
        else
            fail "libcamera does not list this camera (rpicam-hello --list-cameras)"
            hint "Check the libcamera installation, see docs/libcamera.md"
        fi
    else
        info "libcamera disabled (dtparam=${cam}_libcamera_on to enable ISP / rpicam-apps / libcamerasrc)"
    fi

    # --- streaming test ---------------------------------------------------
    $STREAM_TEST || continue
    trigger=$(v4l2-ctl -d "$subdev" --get-ctrl=trigger_mode 2>/dev/null | awk '{print $2}')
    if [ -n "$trigger" ] && [ "$trigger" != "0" ]; then
        warn "Streaming test skipped: trigger_mode=$trigger (frames only arrive with a trigger)"
        continue
    fi

    tmp=$(mktemp -d)
    if [ -n "$libcam_idx" ]; then
        out=$(timeout -k 2 10 rpicam-vid -n --camera "$libcam_idx" -t 3000 --codec yuv420 -o /dev/null \
              --save-pts "$tmp/pts.txt" 2>&1)
        frames=$(grep -c '^[0-9]' "$tmp/pts.txt" 2>/dev/null)
        # fps from the median frame interval (timestamps in ms)
        fps=$(awk '/^[0-9]/ { if (n++) print $1 - p; p = $1 }' "$tmp/pts.txt" 2>/dev/null | sort -n \
              | awk '{ d[NR] = $1 } END { if (NR > 1) printf "%.1f", 1000 / d[int((NR + 1) / 2)] }')
        how="rpicam-vid"
    elif [ "$SOC" = "bcm2712" ]; then
        info "Streaming test skipped: without libcamera the Raspberry Pi 5 pipeline has to be set up first (vc-config)"
        rm -rf "$tmp"; continue
    else
        out=$(timeout -k 2 8 v4l2-ctl -d "$videodev" --stream-mmap --stream-count=60 --stream-to=/dev/null 2>&1)
        frames=$(echo "$out" | tr -cd '<' | wc -c)
        fps=$(echo "$out" | grep -o '[0-9.]* fps' | tail -n 1 | awk '{print $1}')
        how="v4l2-ctl"
    fi
    rm -rf "$tmp"

    if echo "$out" | grep -qiE 'busy|acquire\(\) failed|failed to acquire camera'; then
        warn "Streaming test skipped: camera is in use by another process"
    elif [ "${frames:-0}" -gt 0 ]; then
        ok "Streaming ($how): $frames frames${fps:+, $fps fps}"
    else
        fail "Streaming ($how): no frames received"
        echo "$out" | grep -iE 'error|fail' | tail -n 3 | sed 's/^/         | /'
        hint "Check lanes and cable; kernel log: dmesg | grep vc_mipi"
    fi
done

# ---------------------------------------------------------------------------
title "Summary"
echo "  $n_ok passed, $n_warn warnings, $n_fail failed, $cameras_found camera(s) probed"
if [ $n_fail -gt 0 ]; then
    echo "  If the problem persists, create a support package with tools/collect_support_info.sh"
    exit 1
fi
exit 0
