#!/bin/sh

FFMPEG_VERSION=4.3

if [ ! -d "ffmpeg-${FFMPEG_VERSION}-dist" ]; then
    wget "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz" 2>&1 > /dev/null
    tar fvx "ffmpeg-${FFMPEG_VERSION}.tar.xz" 2>&1 > /dev/null
    mkdir "ffmpeg-${FFMPEG_VERSION}-dist"
    
    cd "ffmpeg-${FFMPEG_VERSION}"
    ./configure --disable-doc --disable-programs
    make -j5
    DESTDIR="../ffmpeg-${FFMPEG_VERSION}-dist" make install
fi


