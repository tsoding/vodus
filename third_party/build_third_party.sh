#!/bin/sh

set -e

# TODO(#89): Integrate ./build_third_party.sh into the main build process
FFMPEG_VERSION=4.3
GIFLIB_VERSION=5.2.1
MAKE=make

# TODO: ./build_third_party.sh does not respect other BSD's or Darwin that might not use GNU make
# Testing is required and the conditions here may have to be changed accordingly.
if [ `uname -s` = "FreeBSD" ]; then
	echo INFO : FreeBSD detected. Setting MAKE to gmake.
	if test -x "/usr/local/bin/gmake"; then
		MAKE=gmake
	else
		echo "GNU Make is required to build the third-party dependencies."
		echo "Aborting..."
		exit 1
	fi
fi

if [ ! -d "ffmpeg-${FFMPEG_VERSION}-dist" ]; then
    wget "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
    tar fvx "ffmpeg-${FFMPEG_VERSION}.tar.xz"
    mkdir "ffmpeg-${FFMPEG_VERSION}-dist"
    
    cd "ffmpeg-${FFMPEG_VERSION}"
      ./configure --disable-doc --disable-programs
      $MAKE -j5
      DESTDIR="../ffmpeg-${FFMPEG_VERSION}-dist" $MAKE install
    cd ..
fi

if [ ! -d "giflib-${GIFLIB_VERSION}-dist" ]; then
    wget "https://deac-riga.dl.sourceforge.net/project/giflib/giflib-${GIFLIB_VERSION}.tar.gz"
    tar fvx "giflib-${GIFLIB_VERSION}.tar.gz"
    mkdir "giflib-${GIFLIB_VERSION}-dist"

    cd "giflib-${GIFLIB_VERSION}"
      $MAKE -j5
      DESTDIR="../giflib-${GIFLIB_VERSION}-dist" $MAKE install
    cd ..
fi

