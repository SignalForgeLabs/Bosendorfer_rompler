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

VST3_SRC=$(find "build/BosendorferImperial_artefacts/Release" "build/BosendorferImperial_artefacts" -name "Bosendorfer Imperial 290.vst3" -type d 2>/dev/null | head -n 1)
STANDALONE_SRC=$(find "build/BosendorferImperial_artefacts/Release" "build/BosendorferImperial_artefacts" -name "Bosendorfer Imperial 290" -type f 2>/dev/null | head -n 1)

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
rm -f "$DEST_BIN/bosendorfer-imperial-290"
cp "$SCRIPT_DIR/Bosendorfer Imperial 290" "$DEST_BIN/bosendorfer-imperial-290"
chmod 755 "$DEST_BIN/bosendorfer-imperial-290"

# Ensure default ALSA audio output configuration exists for Standalone
SETTINGS_FILE="$HOME/.config/Bosendorfer Imperial 290.settings"
mkdir -p "$HOME/.config"
if [ ! -f "$SETTINGS_FILE" ] || grep -q 'name="audioSetup" val=""' "$SETTINGS_FILE" 2>/dev/null; then
    cat > "$SETTINGS_FILE" << 'SETTINGS_EOF'
<?xml version="1.0" encoding="UTF-8"?>

<PROPERTIES>
  <VALUE name="windowX" val="460"/>
  <VALUE name="windowY" val="180"/>
  <VALUE name="audioSetup">
    <DEVICESETUP deviceType="ALSA" audioOutputDeviceName="default" audioInputDeviceName=""
                 audioDeviceRate="44100.0" audioDeviceBufferSize="512" audioDeviceOutChans="11"/>
  </VALUE>
  <VALUE name="shouldMuteInput" val="1"/>
</PROPERTIES>
SETTINGS_EOF
fi

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

# Build self-extracting installable binary (.run)
RUN_INSTALLER="$DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}_Installer.run"
cat << 'RUN_HEADER_EOF' > "$RUN_INSTALLER"
#!/usr/bin/env bash
set -e
echo "=========================================================="
echo " Bosendorfer Imperial 290 v1.0.0 Self-Extracting Installer"
echo " by SignalForge Audio Labs"
echo "=========================================================="
TMP_EXTRACT="$(mktemp -d /tmp/bosendorfer_installer.XXXXXX)"
trap 'rm -rf "$TMP_EXTRACT"' EXIT
ARCHIVE_LINE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0; }' "$0")
tail -n +"$ARCHIVE_LINE" "$0" | tar -xz -C "$TMP_EXTRACT"
bash "$TMP_EXTRACT/Bosendorfer_Imperial_290_Linux_v1.0.0/install.sh"
exit 0
__ARCHIVE_BELOW__
RUN_HEADER_EOF
cat "Bosendorfer_Imperial_290_Linux_v${VERSION}.tar.gz" >> "$RUN_INSTALLER"
chmod +x "$RUN_INSTALLER"

# Build Debian/Ubuntu/Mint installable package (.deb)
DEB_STAGE="$DIST_DIR/deb_stage"
rm -rf "$DEB_STAGE"
mkdir -p "$DEB_STAGE/DEBIAN"
mkdir -p "$DEB_STAGE/usr/bin"
mkdir -p "$DEB_STAGE/usr/lib/vst3"
mkdir -p "$DEB_STAGE/usr/share/applications"

cp "$REPO_ROOT/$STANDALONE_SRC" "$DEB_STAGE/usr/bin/bosendorfer-imperial-290"
chmod 755 "$DEB_STAGE/usr/bin/bosendorfer-imperial-290"
cp -R "$REPO_ROOT/$VST3_SRC" "$DEB_STAGE/usr/lib/vst3/Bosendorfer Imperial 290.vst3"
chmod -R 755 "$DEB_STAGE/usr/lib/vst3/Bosendorfer Imperial 290.vst3"

cat << 'DEB_DESKTOP_EOF' > "$DEB_STAGE/usr/share/applications/bosendorfer-imperial-290.desktop"
[Desktop Entry]
Version=1.0
Type=Application
Name=Bosendorfer Imperial 290
Comment=Bosendorfer Imperial 290 by SignalForge Audio Labs
Exec=/usr/bin/bosendorfer-imperial-290
Icon=audio-card
Terminal=false
Categories=AudioVideo;Audio;Music;
StartupNotify=true
DEB_DESKTOP_EOF

INSTALLED_KB=$(du -sk "$DEB_STAGE/usr" | cut -f1)
cat << DEB_CONTROL_EOF > "$DEB_STAGE/DEBIAN/control"
Package: bosendorfer-imperial-290
Version: ${VERSION}
Section: sound
Priority: optional
Architecture: amd64
Installed-Size: ${INSTALLED_KB}
Maintainer: SignalForge Audio Labs <support@sfalabs.digital>
Description: Bosendorfer Imperial 290 Concert Grand by SignalForge Audio Labs
 97-Key Concert Grand Piano Rompler with dual-microphone convolution and room IRs.
 Includes both the Standalone application and the VST3 plugin.
DEB_CONTROL_EOF

DEB_OUT="$DIST_DIR/bosendorfer-imperial-290_${VERSION}_amd64.deb"
if command -v dpkg-deb >/dev/null 2>&1; then
    dpkg-deb --root-owner-group --build "$DEB_STAGE" "$DEB_OUT"
else
    python3 - "$DEB_STAGE" "$DEB_OUT" << 'PY_DEB_EOF'
import os, sys, tarfile, time

stage_dir, deb_path = sys.argv[1], sys.argv[2]
control_tar = os.path.join(stage_dir, "control.tar.gz")
data_tar = os.path.join(stage_dir, "data.tar.gz")

def reset_tarinfo(ti):
    ti.uid = ti.gid = 0
    ti.uname = ti.gname = "root"
    return ti

with tarfile.open(control_tar, "w:gz") as tf:
    tf.add(os.path.join(stage_dir, "DEBIAN", "control"), arcname="./control", filter=reset_tarinfo)

with tarfile.open(data_tar, "w:gz") as tf:
    tf.add(os.path.join(stage_dir, "usr"), arcname="./usr", filter=reset_tarinfo)

def ar_member(name, data):
    hdr = f"{name:<16}{int(time.time()):<12}0     0     100644  {len(data):<10}`\n".encode("ascii")
    pad = b"\n" if len(data) % 2 == 1 else b""
    return hdr + data + pad

with open(deb_path, "wb") as f:
    f.write(b"!<arch>\n")
    f.write(ar_member("debian-binary", b"2.0\n"))
    with open(control_tar, "rb") as cf:
        f.write(ar_member("control.tar.gz", cf.read()))
    with open(data_tar, "rb") as df:
        f.write(ar_member("data.tar.gz", df.read()))
PY_DEB_EOF
fi
rm -rf "$DEB_STAGE"

echo "=========================================================="
echo " Linux packages created:"
echo " - $RUN_INSTALLER"
echo " - $DEB_OUT"
echo " - $DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}.tar.gz"
echo " - $DIST_DIR/Bosendorfer_Imperial_290_Linux_v${VERSION}.zip"
echo "=========================================================="

