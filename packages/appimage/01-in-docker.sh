#!/bin/bash

set -euxo pipefail

if [[ ! -v ARCH || ! -v APPIMAGE_RUNTIME ]]; then
  echo 'This script should be run in a given container.'
  exit 1
fi

. version.inc

SRC_DIR="$PWD"
TEST_VERSION=$(git rev-list HEAD --count)
if [[ -n "$APP_VERSION_SUFFIX" ]]; then
  VERSION="$APP_VERSION.$TEST_VERSION.$APP_VERSION_SUFFIX"
else
  VERSION="$APP_VERSION.$TEST_VERSION"
fi

APPIMAGE_FILE=RedPandaIDE-$VERSION-$ARCH.AppImage
RUNTIME_SIZE=$(wc -c <$APPIMAGE_RUNTIME)

# build RedPanda C++
mkdir -p /build
cd /build
qmake PREFIX=/usr QMAKE_LFLAGS=-static "$SRC_DIR/Red_Panda_CPP.pro"
make -j$(nproc)

# install RedPanda C++ to AppDir
make INSTALL_ROOT=/RedPandaIDE.AppDir install

# setup AppImage resource
cd /RedPandaIDE.AppDir
ln -s usr/share/applications/RedPandaIDE.desktop RedPandaIDE.desktop
ln -s usr/share/icons/hicolor/scalable/apps/redpandaide.svg redpandaide.svg
# following files may come from Windows filesystem, use `install` to preseve file permission
install -m755 "$SRC_DIR/packages/appimage/AppRun.sh" AppRun
install -m644 "$SRC_DIR/platform/linux/redpandaide.png" .DirIcon

# create AppImage
cd /
mksquashfs RedPandaIDE.AppDir $APPIMAGE_FILE -offset $RUNTIME_SIZE -comp zstd -root-owned -noappend -b 1M -mkfs-time 0
dd if=$APPIMAGE_RUNTIME of=$APPIMAGE_FILE conv=notrunc
chmod +x $APPIMAGE_FILE

# copy back to host
mkdir -p "$SRC_DIR/dist"
cp $APPIMAGE_FILE "$SRC_DIR/dist"
