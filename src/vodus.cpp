#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <ctime>

#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <gif_lib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "./stb_image.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include "./aids.hpp"
using namespace aids;

const size_t VODUS_MESSAGES_CAPACITY = 1024;

// PLEASE READ THIS --> https://en.wikipedia.org/wiki/Single_Compilation_Unit
#include "./vodus_image32.cpp"
#include "./vodus_video_params.cpp"
#include "./vodus_emotes.cpp"
#include "./vodus_main.cpp"
