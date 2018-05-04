#!/bin/bash

SOURCE_DIR=$PWD/../../
TEMP_DIR=$(mktemp -d)
BUILD_DIR=$TEMP_DIR/build
DEB_DIR=$BUILD_DIR/deb

trap "rm -rf $TEMP_DIR" EXIT

# Build edb
mkdir -p $BUILD_DIR
pushd $BUILD_DIR
cmake $SOURCE_DIR
if ! make -j8; then
	echo "Compiling error. Exiting..."
	exit 1
fi

# Install it into a temp directory
make DESTDIR=$DEB_DIR install

# Getting Arch
if [ $(uname -m) == "x86_64" ]; then
	ARCH="amd64"
else
	ARCH="i386"
fi

INSTALL_SIZE=$(du -b -s $DEB_DIR | cut -f1)
VERSION=$($BUILD_DIR/edb --dump-version 2> /dev/null)

#TODO(eteran): figure out the proper deps for Qt5 on Ubuntu/Debian
DEPENDS="libqt5core5a (>= 5.0.0), libqt5gui5 (>= 5.0.0), libcapstone3"

# Create the meta-data dir
mkdir -p $DEB_DIR/DEBIAN

# MD5s 
# TODO(eteran): do we need to get rid of the prefix on the files here?
find $DEB_DIR -type f | xargs md5sum > $DEB_DIR/DEBIAN/md5sums

# Generating Debian control file
echo "Package: edb-debugger-qt5
Version: $VERSION
Architecture: $ARCH
Maintainer: Evan Teran
Homepage: http://www.codef00.com
Installed-Size: $INSTALL_SIZE
Depends: $DEPENDS
Provides: edb
Section: devel
Priority: extra
Description: Graphical debugger and disassembler for ELF binaries
 EDB (Evan's Debugger) is a modular and modern disassembler and debugger for
 binary ELF files based on ptrace API and the capstone disassembly library. 
 EDB is very similar to OllyDbg, a famous freeware debugger for PE 
 (Portable Executable) files. The intent of EDB is to debug binaries without 
 source code. It's possible to set conditional and inconditional breakpoints, 
 display memory stack, processor registers state and more. The power of EDB can 
 be increased with many plugins." > $DEB_DIR/DEBIAN/control

# Generate package
dpkg-deb -b $DEB_DIR $SOURCE_DIR/edb-debugger-qt5_${VERSION}_${ARCH}.deb
