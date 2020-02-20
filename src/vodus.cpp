#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <gif_lib.h>
#include <png.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

// https://en.wikipedia.org/wiki/Single_Compilation_Unit
#include "./vodus_image32.cpp"
#include "./vodus_main.cpp"
