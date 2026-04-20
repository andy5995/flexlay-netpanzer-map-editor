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
    meson \
    ninja-build \
    qtbase5-dev \
    qtbase5-dev-tools \
    qttools5-dev-tools \
    libqt5gui5 \
    pkg-config \
    patchelf \
"

# ---------------------------------------------------------------------------
# Build and install into AppDir
# ---------------------------------------------------------------------------

cd "$WORKSPACE"
# --wipe forces a clean reconfigure so a pre-existing build/ directory
# (e.g. from a host build with different Qt version or sanitizer flags)
# does not carry stale settings into the container environment.
meson setup build --buildtype=release --wipe --prefix=/usr
ninja -C build

# Install binary, desktop file, icon, and docs directly into the AppDir.
# meson respects --prefix=/usr and DESTDIR so files land under $APPDIR/usr/.
meson install -C build --destdir "$APPDIR"

# ---------------------------------------------------------------------------
# linuxdeploy — bundle shared library dependencies (Qt5 + system libs)
# ---------------------------------------------------------------------------

OUT_DIR="$WORKSPACE/out"
mkdir -p "$OUT_DIR"
cd "$OUT_DIR"

ARCH=$(uname -m)
export LINUXDEPLOY_OUTPUT_VERSION="$VERSION"

linuxdeploy \
  --appdir "$APPDIR" \
  --executable "$APPDIR/usr/bin/netpanzer-editor" \
  --desktop-file "$APPDIR/usr/share/applications/netpanzer-editor.desktop" \
  --icon-file "$APPDIR/usr/share/pixmaps/netpanzer-editor.png" \
  --icon-filename netpanzer-editor \
  --custom-apprun "$WORKSPACE/AppRun" \
  --plugin qt

# ---------------------------------------------------------------------------
# Pack the AppImage
# ---------------------------------------------------------------------------

OUT_APPIMAGE="netpanzer-editor-$VERSION-$ARCH.AppImage"

GITHUB_REPOSITORY_OWNER="${GITHUB_REPOSITORY_OWNER:-netpanzer}"
REPO="${GITHUB_REPOSITORY:-flexlay-netpanzer-map-editor}"
# Strip owner prefix if GITHUB_REPOSITORY is in "owner/repo" form
REPO="${REPO##*/}"
# Snapshot builds are published under the "snapshot" tag; tagged releases use the tag itself.
if [[ "$VERSION" == "snapshot" || -z "$VERSION" ]]; then
  TAG="snapshot"
else
  TAG="$VERSION"
fi
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
