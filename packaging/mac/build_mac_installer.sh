#!/bin/bash
set -e

# ==============================================================================
# SignalForgeLabs Bosendorfer Imperial 290 - macOS Universal Binary Installer
# ==============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

VERSION="1.0.0"
BUNDLE_ID="com.SignalForgeLabs.BosendorferImperial290"
DIST_DIR="$REPO_ROOT/dist/mac"
BUILD_DIR="$REPO_ROOT/build"
TMP_DIR="$REPO_ROOT/build/installer_staging"

echo "======================================================="
echo " Building macOS Universal Audio Plugin Installer v$VERSION"
echo "======================================================="

# 1. Configure and Build Universal Binary (Intel + Apple Silicon)
echo "[1/5] Configuring CMake with Universal Binary (arm64 + x86_64)..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="10.13"

echo "[2/5] Compiling Release binaries..."
cmake --build "$BUILD_DIR" --config Release -j$(sysctl -n hw.ncpu || echo 4)

# Locate artifacts
VST3_SRC=$(find "$BUILD_DIR" -name "Bosendorfer Imperial 290.vst3" -type d | head -n 1)
AU_SRC=$(find "$BUILD_DIR" -name "Bosendorfer Imperial 290.component" -type d | head -n 1)
APP_SRC=$(find "$BUILD_DIR" -name "Bosendorfer Imperial 290.app" -type d | head -n 1)

echo "Located artifacts:"
echo " - VST3: $VST3_SRC"
echo " - AU:   $AU_SRC"
echo " - APP:  $APP_SRC"

if [ -z "$VST3_SRC" ] || [ ! -d "$VST3_SRC" ]; then
    echo "ERROR: Bosendorfer Imperial 290.vst3 was not generated in $BUILD_DIR"
    exit 1
fi
if [ -z "$AU_SRC" ] || [ ! -d "$AU_SRC" ]; then
    echo "ERROR: Bosendorfer Imperial 290.component was not generated in $BUILD_DIR"
    exit 1
fi
if [ -z "$APP_SRC" ] || [ ! -d "$APP_SRC" ]; then
    echo "ERROR: Bosendorfer Imperial 290.app was not generated in $BUILD_DIR"
    exit 1
fi

# 2. Prepare staging directories
echo "[3/5] Setting up staging directory structure..."
rm -rf "$TMP_DIR"
mkdir -p "$TMP_DIR/packages"
mkdir -p "$DIST_DIR"

# Stage VST3
mkdir -p "$TMP_DIR/root_vst3/Library/Audio/Plug-Ins/VST3"
cp -R "$VST3_SRC" "$TMP_DIR/root_vst3/Library/Audio/Plug-Ins/VST3/"
pkgbuild --root "$TMP_DIR/root_vst3" \
         --identifier "${BUNDLE_ID}.vst3" \
         --version "$VERSION" \
         --install-location "/" \
         "$TMP_DIR/packages/Bosendorfer_VST3.pkg"

# Stage AU (Component)
mkdir -p "$TMP_DIR/root_au/Library/Audio/Plug-Ins/Components"
cp -R "$AU_SRC" "$TMP_DIR/root_au/Library/Audio/Plug-Ins/Components/"
pkgbuild --root "$TMP_DIR/root_au" \
         --identifier "${BUNDLE_ID}.au" \
         --version "$VERSION" \
         --install-location "/" \
         "$TMP_DIR/packages/Bosendorfer_AU.pkg"

# Stage Standalone Application
mkdir -p "$TMP_DIR/root_app/Applications"
cp -R "$APP_SRC" "$TMP_DIR/root_app/Applications/"
pkgbuild --root "$TMP_DIR/root_app" \
         --identifier "${BUNDLE_ID}.app" \
         --version "$VERSION" \
         --install-location "/" \
         "$TMP_DIR/packages/Bosendorfer_Standalone.pkg"

# Stage Documentation
mkdir -p "$TMP_DIR/root_docs/Library/Audio/Presets/SignalForgeLabs/BosendorferImperial290"
cp "$REPO_ROOT/packaging/common/README_INSTALL.txt" "$TMP_DIR/root_docs/Library/Audio/Presets/SignalForgeLabs/BosendorferImperial290/README.txt"
pkgbuild --root "$TMP_DIR/root_docs" \
         --identifier "${BUNDLE_ID}.docs" \
         --version "$VERSION" \
         --install-location "/" \
         "$TMP_DIR/packages/Bosendorfer_Support.pkg"

# 3. Create Multi-Component Distribution Package (.pkg)
echo "[4/5] Building distribution package with productbuild..."
PKG_OUT="$DIST_DIR/Bosendorfer_Imperial_290_macOS_Installer_v${VERSION}.pkg"
productbuild --distribution "$SCRIPT_DIR/distribution.xml" \
             --package-path "$TMP_DIR/packages" \
             --resources "$SCRIPT_DIR" \
             "$PKG_OUT"

# 4. Wrap into distributable Disk Image (.dmg)
echo "[5/5] Creating distributable DMG disk image..."
DMG_STAGING="$TMP_DIR/dmg_root"
mkdir -p "$DMG_STAGING"
cp "$PKG_OUT" "$DMG_STAGING/"
cp "$REPO_ROOT/packaging/common/README_INSTALL.txt" "$DMG_STAGING/README.txt"

DMG_OUT="$DIST_DIR/Bosendorfer_Imperial_290_macOS_v${VERSION}.dmg"
rm -f "$DMG_OUT"
hdiutil create -volname "Bosendorfer Imperial 290" \
               -srcfolder "$DMG_STAGING" \
               -ov -format UDZO \
               "$DMG_OUT"

rm -rf "$TMP_DIR"

echo "======================================================="
echo " SUCCESS! macOS distribution deliverables generated:"
echo " 1. Package:    $PKG_OUT"
echo " 2. Disk Image: $DMG_OUT"
echo "======================================================="
