#!/bin/sh

FFMPEG_VERSION=4.3

wget "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz" 2>&1 > /dev/null
tar fvx "ffmpeg-${FFMPEG_VERSION}.tar.xz" 2>&1 > /dev/null
cd "ffmpeg-${FFMPEG_VERSION}"
./configure
make -j5
