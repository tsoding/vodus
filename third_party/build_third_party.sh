#!/bin/sh

FFMPEG_VERSION=4.3
GIFLIB_VERSION=5.2.1

if [ ! -d "ffmpeg-${FFMPEG_VERSION}-dist" ]; then
    wget "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"
    tar fvx "ffmpeg-${FFMPEG_VERSION}.tar.xz"
    mkdir "ffmpeg-${FFMPEG_VERSION}-dist"
    
    cd "ffmpeg-${FFMPEG_VERSION}"
      ./configure --disable-doc --disable-programs
      make -j5
      DESTDIR="../ffmpeg-${FFMPEG_VERSION}-dist" make install
    cd ..
fi

if [ ! -d "giflib-${GIFLIB_VERSION}-dist" ]; then
    wget "https://deac-riga.dl.sourceforge.net/project/giflib/giflib-${GIFLIB_VERSION}.tar.gz"
    tar fvx "giflib-${GIFLIB_VERSION}.tar.gz"
    mkdir "giflib-${GIFLIB_VERSION}-dist"

    cd "giflib-${GIFLIB_VERSION}"
      make -j5
      DESTDIR="../giflib-${GIFLIB_VERSION}-dist" make install
    cd ..
fi

