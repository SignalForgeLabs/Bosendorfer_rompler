#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

VERSION="1.0.0"
DIST_DIR="$REPO_ROOT/dist/linux"
PKG_DIR="$DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}"

echo "=========================================================="
echo " Packaging Linux Distribution v${VERSION}"
echo "=========================================================="

VST3_SRC=$(find "build/BosendorferImperial_artefacts" -name "Bosendorfer Imperial 290.vst3" -type d | head -n 1)
STANDALONE_SRC=$(find "build/BosendorferImperial_artefacts" -name "Bosendorfer Imperial 290" -type f | head -n 1)

if [ -z "$VST3_SRC" ] || [ ! -d "$VST3_SRC" ]; then
    echo "ERROR: Bosendorfer Imperial 290.vst3 was not found in build/BosendorferImperial_artefacts!"
    ls -laR build/BosendorferImperial_artefacts 2>/dev/null || true
    exit 1
fi

if [ -z "$STANDALONE_SRC" ] || [ ! -f "$STANDALONE_SRC" ]; then
    echo "ERROR: Bosendorfer Imperial 290 standalone executable was not found in build/BosendorferImperial_artefacts!"
    ls -laR build/BosendorferImperial_artefacts 2>/dev/null || true
    exit 1
fi

echo "Found Linux artifacts:"
echo " - VST3:       $VST3_SRC"
echo " - Standalone: $STANDALONE_SRC"

rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"

cp -R "$VST3_SRC" "$PKG_DIR/Bosendorfer Imperial 290.vst3"
cp "$STANDALONE_SRC" "$PKG_DIR/Bosendorfer Imperial 290"
chmod +x "$PKG_DIR/Bosendorfer Imperial 290"
cp packaging/common/README_INSTALL.txt "$PKG_DIR/README.txt"

cat << 'INSTALL_EOF' > "$PKG_DIR/install.sh"
#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "=========================================================="
echo " Installing Bosendorfer Imperial 290 (Linux)"
echo "=========================================================="

# 1. Install VST3 plugin
DEST_VST3="$HOME/.vst3"
mkdir -p "$DEST_VST3"
echo "[1/3] Installing VST3 plugin to $DEST_VST3/Bosendorfer Imperial 290.vst3 ..."
rm -rf "$DEST_VST3/Bosendorfer Imperial 290.vst3"
cp -R "$SCRIPT_DIR/Bosendorfer Imperial 290.vst3" "$DEST_VST3/"
chmod -R 755 "$DEST_VST3/Bosendorfer Imperial 290.vst3"

# 2. Install Standalone binary
DEST_BIN="$HOME/.local/bin"
mkdir -p "$DEST_BIN"
echo "[2/3] Installing standalone executable to $DEST_BIN/bosendorfer-imperial-290 ..."
cp "$SCRIPT_DIR/Bosendorfer Imperial 290" "$DEST_BIN/bosendorfer-imperial-290"
chmod 755 "$DEST_BIN/bosendorfer-imperial-290"

# 3. Create desktop launcher shortcut
DESKTOP_DIR="$HOME/Desktop"
APPS_DIR="$HOME/.local/share/applications"
mkdir -p "$APPS_DIR"

cat << DESKTOP_EOF > "$APPS_DIR/bosendorfer-imperial-290.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=Bosendorfer Imperial 290
Comment=SignalForgeLabs Bosendorfer Imperial 290 Concert Grand
Exec=$DEST_BIN/bosendorfer-imperial-290
Icon=audio-card
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupNotify=true
DESKTOP_EOF
chmod +x "$APPS_DIR/bosendorfer-imperial-290.desktop"

if [ -d "$DESKTOP_DIR" ]; then
    cp "$APPS_DIR/bosendorfer-imperial-290.desktop" "$DESKTOP_DIR/Bosendorfer_Imperial_290.desktop"
    chmod +x "$DESKTOP_DIR/Bosendorfer_Imperial_290.desktop"
    gio set "$DESKTOP_DIR/Bosendorfer_Imperial_290.desktop" metadata::trusted true 2>/dev/null || true
fi

echo "[3/3] Created Desktop and Application menu shortcuts."
echo "=========================================================="
echo " Installation complete!"
echo " - VST3: $DEST_VST3/Bosendorfer Imperial 290.vst3"
echo " - Standalone: $DEST_BIN/bosendorfer-imperial-290"
echo "=========================================================="
INSTALL_EOF
chmod +x "$PKG_DIR/install.sh"

cd "$DIST_DIR"
tar -czvf "Bosendorfer_Imperial_290_Linux_v${VERSION}.tar.gz" "Bosendorfer_Imperial_290_Linux_v${VERSION}"
zip -r "Bosendorfer_Imperial_290_Linux_v${VERSION}.zip" "Bosendorfer_Imperial_290_Linux_v${VERSION}"

echo "=========================================================="
echo " Linux packages created:"
echo " - $DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}.tar.gz"
echo " - $DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}.zip"
echo "=========================================================="
