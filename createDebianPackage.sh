sudo rm -rf build
mkdir -p build

# Set version if not set
if [ -z "$VERSION_DEB_PACKAGE" ]; then
    export VERSION_DEB_PACKAGE="0.5.3"
fi
# Delete v from version
export VERSION_DEB_PACKAGE=$(echo $VERSION_DEB_PACKAGE | sed 's/v//')


cp -r debian_package build/debian
mkdir -p build/debian
cp -r src build/


envsubst '$VERSION_DEB_PACKAGE' < debian_package/changelog > build/debian/changelog
envsubst '$VERSION_DEB_PACKAGE' < dkms.conf > build/dkms.conf
envsubst '$VERSION_DEB_PACKAGE' < debian_package/control > build/debian/control
envsubst '$VERSION_DEB_PACKAGE' < debian_package/not-installed > build/debian/not-installed
envsubst '$VERSION_DEB_PACKAGE' < debian_package/postinst > build/debian/postinst
envsubst '$VERSION_DEB_PACKAGE' < debian_package/postrm > build/debian/postrm
envsubst '$VERSION_DEB_PACKAGE' < debian_package/rules > build/debian/rules
envsubst < debian_package/vc-mipi-driver-bcm2712.install > build/debian/vc-mipi-driver-bcm2712.install 

cd build
sudo dpkg-buildpackage -us -uc -b
