#!/bin/bash
# Legacy DMG creation script
# 
# NOTE: This script is deprecated. Use CMake/CPack instead:
#   cmake --build build --target dmg
# Or manually:
#   cd build && cpack -G DragNDrop

set -e

# Detect build directory
if [ -d "build-vcpkg/bin/dunelegacy.app" ]; then
    APP_PATH="build-vcpkg/bin/dunelegacy.app"
elif [ -d "build/bin/dunelegacy.app" ]; then
    APP_PATH="build/bin/dunelegacy.app"
elif [ -d "IDE/xCode/build/Release/Dune Legacy.app" ]; then
    APP_PATH="IDE/xCode/build/Release/Dune Legacy.app"
else
    echo "Error: Could not find dunelegacy.app in any build directory"
    echo "Please build the project first with:"
    echo "  cmake --build build --config Release"
    exit 1
fi

# Extract version from app bundle
VERSION=$(defaults read "$(pwd)/${APP_PATH}/Contents/Info.plist" CFBundleShortVersionString)
DMG_NAME="DuneLegacy-${VERSION}-macOS.dmg"
VOLUME_NAME="Dune Legacy ${VERSION}"

echo "Creating DMG from: $APP_PATH"
echo "Output: $DMG_NAME"

# Create a temporary directory for DMG contents
TEMP_DIR=$(mktemp -d)
mkdir -p "$TEMP_DIR/Dune Legacy"

# Copy the app bundle
echo "Copying app bundle..."
cp -R "$APP_PATH" "$TEMP_DIR/Dune Legacy/"

# Create symlink to Applications folder
echo "Creating Applications symlink..."
ln -s /Applications "$TEMP_DIR/Dune Legacy/"

# Create DMG
echo "Creating DMG (compressed)..."
hdiutil create -volname "$VOLUME_NAME" -srcfolder "$TEMP_DIR/Dune Legacy" -ov -format UDZO "$DMG_NAME"

# Clean up
rm -rf "$TEMP_DIR"

echo "✅ DMG created successfully: $DMG_NAME"
echo ""
echo "💡 TIP: Use 'cmake --build build --target dmg' for integrated build+package" 