#!/bin/bash
# Installation script for vc-set-format utility

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR"

echo "Building vc-set-format..."
cd "$BUILD_DIR"
make clean
make

echo ""
echo "vc-set-format built successfully!"
echo ""
echo "To test it:"
echo "  $BUILD_DIR/vc-set-format --help"
echo "  $BUILD_DIR/vc-set-format -l /dev/video0"
echo ""
echo "To install system-wide (optional):"
echo "  sudo make install"
echo ""
echo "Integration with vc-config:"
echo "  The vc-config script can be updated to use this utility by replacing:"
echo "  v4l2-ctl -d \"\$videodev\" --set-fmt-video=width=\$width,height=\$height,pixelformat=\"\$fmt\",colorspace=srgb"
echo ""
echo "  With:"
echo "  \"$BUILD_DIR/vc-set-format\" \"\$videodev\" \"\$width\" \"\$height\" \"\$fmt\" srgb"
echo ""
