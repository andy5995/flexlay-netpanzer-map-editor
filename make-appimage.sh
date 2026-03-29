#!/bin/bash

set -ev

if [ -z "$DOCKER_BUILD" ]; then
  echo "This script is only meant to be used with the linuxdeploy build"
  echo "helper container."
  echo "See docker-compose.yml and the README for details."
  exit 1
fi

if [ -z "$VERSION" ]; then
  echo "VERSION must be set (e.g. 0.1)"
  exit 1
fi

if [[ "$WORKSPACE" != /* ]]; then
  echo "The workspace path must be absolute"
  exit 1
fi

test -d "$WORKSPACE"

APPDIR="${APPDIR:-/tmp/${USER:-build}-AppDir}"

if [ -d "$APPDIR" ]; then
  rm -rf "$APPDIR"
fi
mkdir -vp "$APPDIR"

env
export -p

cd "$WORKSPACE"

if [ ! -e "AppRun" ]; then
  echo "You must be in the same directory where the AppRun file resides"
  exit 1
fi

# ---------------------------------------------------------------------------
# Install build-time dependencies
# ---------------------------------------------------------------------------

sudo DEBIAN_FRONTEND=noninteractive sh -c "
  apt-get update && apt-get -y upgrade && \
  apt-get install -y \
    build-essential \
    libboost-dev \
    ruby-dev \
    scons \
    swig \
    libx11-dev \
    libxmu-dev \
    libxi-dev \
    libxxf86vm-dev \
    libxrandr-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libpng-dev \
    libjpeg-dev \
    patchelf \
"

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

cd "$WORKSPACE/external/clanlib"
scons

cd "$WORKSPACE"
scons

# ---------------------------------------------------------------------------
# Populate AppDir
# ---------------------------------------------------------------------------

# Ruby interpreter — bundle so the AppImage is independent of host Ruby version.
# The .so extension modules are ABI-tied to the Ruby version they were compiled
# against; bundling that same Ruby avoids version mismatch errors on the host.
RUBY_BIN=$(command -v ruby)
RUBY_LIBDIR=$(ruby -r rbconfig -e 'puts RbConfig::CONFIG["rubylibdir"]')
RUBY_ARCHDIR=$(ruby -r rbconfig -e 'puts RbConfig::CONFIG["archdir"]')

install -Dm755 "$RUBY_BIN" "$APPDIR/usr/bin/ruby"

mkdir -p "$APPDIR/usr/lib/ruby"
cp -a "$RUBY_LIBDIR/." "$APPDIR/usr/lib/ruby/"
cp -a "$RUBY_ARCHDIR/." "$APPDIR/usr/lib/ruby/"

# Ruby extension modules — placed in usr/lib/ so LD_LIBRARY_PATH and RUBYLIB
# both resolve them correctly from the AppRun environment.
install -Dm755 "$WORKSPACE/ruby/flexlay_wrap.so"        "$APPDIR/usr/lib/flexlay_wrap.so"
install -Dm755 "$WORKSPACE/netpanzer/netpanzer_wrap.so" "$APPDIR/usr/lib/netpanzer_wrap.so"

# Ruby library scripts
mkdir -p "$APPDIR/usr/share/netpanzer-editor"
cp -a "$WORKSPACE/ruby"      "$APPDIR/usr/share/netpanzer-editor/ruby"
cp -a "$WORKSPACE/netpanzer" "$APPDIR/usr/share/netpanzer-editor/netpanzer"

# Flexlay data (icons, gui.xml, etc.) — referenced via FLEXLAY_DATADIR
cp -a "$WORKSPACE/data" "$APPDIR/usr/share/netpanzer-editor/data"

# Desktop entry
mkdir -p "$APPDIR/usr/share/applications"
cat > "$APPDIR/usr/share/applications/netpanzer-editor.desktop" <<DESKTOP
[Desktop Entry]
Name=NetPanzer Map Editor
Comment=Map editor for the NetPanzer game
Exec=netpanzer-editor
Icon=netpanzer-editor
Type=Application
Categories=Game;
DESKTOP

# Icon — use the 64x64 brush image as a stand-in application icon
install -Dm644 "$WORKSPACE/data/images/brush/brush.png" \
  "$APPDIR/usr/share/pixmaps/netpanzer-editor.png"

DOCDIR="$APPDIR/usr/share/doc/netpanzer-editor"
mkdir -p "$DOCDIR"

# Project license (GPL v3)
install -Dm644 "$WORKSPACE/COPYING"  "$DOCDIR/LICENSE"
install -Dm644 "$WORKSPACE/README"   "$DOCDIR/README"

# Ruby license — BSD 2-clause requires preserving the copyright notice in
# binary distributions. Ruby is dual-licensed; the BSD 2-clause text is
# shipped in the distro package's copyright file.
RUBY_COPYRIGHT=$(ls /usr/share/doc/ruby*/copyright 2>/dev/null | head -1)
if [ -z "$RUBY_COPYRIGHT" ]; then
  echo "ERROR: cannot find Ruby copyright file in /usr/share/doc/ruby*/" >&2
  exit 1
fi
install -Dm644 "$RUBY_COPYRIGHT" "$DOCDIR/LICENSE.Ruby"

# ClanLib license — zlib license requires the notice be preserved in
# distributions. Clause 2 also requires modified versions be plainly marked;
# the notice below satisfies both for binary distributions.
install -Dm644 "$WORKSPACE/external/clanlib/doc/COPYING" \
  "$DOCDIR/LICENSE.ClanLib"
cat >> "$DOCDIR/LICENSE.ClanLib" <<'NOTE'

---
NOTE: This copy of ClanLib has been modified from the original source.
Modifications are available at https://github.com/netpanzer/flexlay-netpanzer-map-editor
under external/clanlib/.
NOTE


# ---------------------------------------------------------------------------
# linuxdeploy — bundle shared library dependencies
# ---------------------------------------------------------------------------

cd "$WORKSPACE"
OUT_DIR="$WORKSPACE/out"
mkdir -p "$OUT_DIR"
cd "$OUT_DIR"

ARCH=$(uname -m)
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"

linuxdeploy \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/ruby" \
  --library "$APPDIR/usr/lib/flexlay_wrap.so" \
  --library "$APPDIR/usr/lib/netpanzer_wrap.so" \
  --desktop-file "$APPDIR/usr/share/applications/netpanzer-editor.desktop" \
  --icon-file "$APPDIR/usr/share/pixmaps/netpanzer-editor.png" \
  --icon-filename netpanzer-editor \
  --custom-apprun "$WORKSPACE/AppRun"

# ---------------------------------------------------------------------------
# Pack the AppImage
# ---------------------------------------------------------------------------

OUT_APPIMAGE="netpanzer-editor-$VERSION-$ARCH.AppImage"

REPO="flexlay-netpanzer-map-editor"
TAG="latest"
GITHUB_REPOSITORY_OWNER="${GITHUB_REPOSITORY_OWNER:-netpanzer}"
UPINFO="gh-releases-zsync|$GITHUB_REPOSITORY_OWNER|$REPO|$TAG|*$ARCH.AppImage.zsync"

appimagetool \
  --comp zstd \
  --mksquashfs-opt -Xcompression-level \
  --mksquashfs-opt 20 \
  -u "$UPINFO" \
  "$APPDIR" "$OUT_APPIMAGE"

sha256sum "$OUT_APPIMAGE" > "$OUT_APPIMAGE.sha256sum"
cat "$OUT_APPIMAGE.sha256sum"

exit 0
