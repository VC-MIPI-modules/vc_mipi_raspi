sudo rm -rf build
mkdir -p build

modules=("bcm2711" "bcm2712" )


# Set version if not set
if [ -z "$VERSION_DEB_PACKAGE" ]; then
    export VERSION_DEB_PACKAGE="0.6.4"
fi


# Delete v from version
export VERSION_DEB_PACKAGE=$(echo $VERSION_DEB_PACKAGE | sed 's/v//')

VC_CAMERA_FILE="src/vc_mipi_camera/vc_mipi_camera.c"
VC_CORE_FILE="src/vc_mipi_core/vc_mipi_core.h"
VC_MODULES_FILE="src/vc_mipi_core/vc_mipi_modules.h"

sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_CAMERA_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_CORE_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_MODULES_FILE

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

    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/changelog > $BUILD_DIR/debian/changelog
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < dkms.conf > $BUILD_DIR/dkms.conf
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/control > $BUILD_DIR/debian/control
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/not-installed > $BUILD_DIR/debian/not-installed
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/postinst > $BUILD_DIR/debian/postinst
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/postrm > $BUILD_DIR/debian/postrm
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/rules > $BUILD_DIR/debian/rules
    envsubst '$VERSION_DEB_PACKAGE $MODULE_VERSION' < $SRC_DEB_DIR/vc-mipi-driver-$module.install > $BUILD_DIR/debian/vc-mipi-driver-$module.install 

    chmod ug+x $BUILD_DIR/debian/postinst
    chmod ug+x $BUILD_DIR/debian/postrm
    chmod ug+x $BUILD_DIR/debian/rules

    cd $BUILD_DIR
    sudo -E dpkg-buildpackage -us -uc -b
    cd ../..

done




