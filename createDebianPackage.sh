sudo rm -rf build
mkdir -p build

# Set version if not set
if [ -z "$VERSION_DEB_PACKAGE" ]; then
    export VERSION_DEB_PACKAGE="0.6.0"
fi
# Delete v from version
export VERSION_DEB_PACKAGE=$(echo $VERSION_DEB_PACKAGE | sed 's/v//')

VC_CAMERA_FILE="src/vc_mipi_camera/vc_mipi_camera.c"
VC_CORE_FILE="src/vc_mipi_core/vc_mipi_core.h"
VC_MODULES_FILE="src/vc_mipi_core/vc_mipi_modules.h"

sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_CAMERA_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_CORE_FILE
sed -i "s/^#define VERSION \".*\"/#define VERSION \"$VERSION_DEB_PACKAGE\"/" $VC_MODULES_FILE


rsync -a --exclude='.env' debian_package/ build/debian/

mkdir -p build/debian
rsync -a --exclude='.env' src/ build/src/

DEB_BUILD_OPTIONS="KERNEL_HEADERS=$KERNEL_HEADERS" 



envsubst '$VERSION_DEB_PACKAGE' < debian_package/changelog > build/debian/changelog
envsubst '$VERSION_DEB_PACKAGE' < dkms.conf > build/dkms.conf
envsubst '$VERSION_DEB_PACKAGE' < debian_package/control > build/debian/control
envsubst '$VERSION_DEB_PACKAGE' < debian_package/not-installed > build/debian/not-installed
envsubst '$VERSION_DEB_PACKAGE' < debian_package/postinst > build/debian/postinst
envsubst '$VERSION_DEB_PACKAGE' < debian_package/postrm > build/debian/postrm
envsubst '$VERSION_DEB_PACKAGE' < debian_package/rules > build/debian/rules
envsubst < debian_package/vc-mipi-driver-bcm2712.install > build/debian/vc-mipi-driver-bcm2712.install 

chmod ug+x build/debian/postinst
chmod ug+x build/debian/postrm
chmod ug+x build/debian/rules

cd build
sudo -E dpkg-buildpackage -us -uc -b
