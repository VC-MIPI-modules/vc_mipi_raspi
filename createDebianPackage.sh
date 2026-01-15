#!/bin/bash
sudo rm -rf build
mkdir -p build

modules=("bcm2712" "bcm2711" "bcm2837" "vccmi10" "rp3a0")


# Set version if not set
if [ -z "$VERSION_DEB_PACKAGE" ]; then
    export VERSION_DEB_PACKAGE="0.6.8"
fi
if [ -z "$VERSION_CORE" ]; then
    export VERSION_CORE="0.6.8"
fi

# Delete v from version
export VERSION_DEB_PACKAGE=$(echo $VERSION_DEB_PACKAGE | sed 's/v//')

VC_CAMERA_FILE="src/vc_mipi_camera/vc_mipi_camera.c"
VC_CORE_FILE="src/vc_mipi_core/vc_mipi_core.h"
VC_MODULES_FILE="src/vc_mipi_core/vc_mipi_modules.h"

sed -i "s/^#define VERSION_CAMERA \".*\"/#define VERSION_CAMERA \"$VERSION_DEB_PACKAGE\"/" $VC_CAMERA_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_CORE\"/" $VC_CORE_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_CORE\"/" $VC_MODULES_FILE


process_debian_template() {
    local src_dir="$1"
    local build_dir="$2"
    local template="$3"
    local dest="$4"

    if [ -f "$src_dir/$template" ]; then
        envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < "$src_dir/$template" > "$build_dir/$dest"
        # Make executable if it's a script
        if [[ "$dest" == "postinst" || "$dest" == "preinst" || "$dest" == "prerm" || "$dest" == "postrm" || "$dest" == "rules" ]]; then
            chmod +x "$build_dir/$dest"
        fi
    fi
}

rm -rf build
mkdir -p build
for module in "${modules[@]}"; do

    export MODULE_VERSION=$module

    BUILD_DIR="build/build-$module"
    echo "DIR: $BUILD_DIR"
    SRC_DEB_DIR="debian_package/debian_package-$module"
    SRC_SCRIPT_DIR="scripts/$module"
    mkdir -p $BUILD_DIR/debian/ 
    mkdir -p $BUILD_DIR/src/

    cp -r $SRC_DEB_DIR/* $BUILD_DIR/debian/
    mkdir -p $BUILD_DIR/src/overlays
    cp -r $SRC_SCRIPT_DIR/* $BUILD_DIR/src/
    make -C overlays/overlays-$module/ all
    cp -r overlays/overlays-$module/* $BUILD_DIR/src/overlays/
    # cp -r overlays/overlays-$module/* $BUILD_DIR/debian/tmp/usr/src/vc-mipi-driver-$module-${VERSION_DEB_PACKAGE}/overlays/
    rsync -a --exclude='.env' src/ $BUILD_DIR/src/

    DEB_BUILD_OPTIONS="KERNEL_HEADERS=$KERNEL_HEADERS" 
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < dkms.conf > $BUILD_DIR/dkms.conf

    debian_templates=(
    "changelog:changelog"
    "control:control"
    "not-installed:not-installed"
    "postinst:postinst"
    "preinst:preinst"
    "prerm:prerm"
    "postrm:postrm"
    "rules:rules"
    "vc-mipi-driver-$module.install:vc-mipi-driver-$module.install"
    )

    for template_pair in "${debian_templates[@]}"; do
        IFS=":" read -r template dest <<< "$template_pair"
        # Expand $module in file names
        template="${template//\$module/$module}"
        dest="${dest//\$module/$module}"
        process_debian_template "$SRC_DEB_DIR" "$BUILD_DIR/debian" "$template" "$dest"
    done

   
    cd $BUILD_DIR || exit 1
    sudo -E dpkg-buildpackage -us -uc -F
    cd ../.. || exit 1

done




