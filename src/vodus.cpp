#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <climits>

#include <algorithm>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <gif_lib.h>

#ifdef VODUS_RELEASE
#    define STB_IMAGE_IMPLEMENTATION
#    define STB_IMAGE_RESIZE_IMPLEMENTATION
#    define STB_IMAGE_WRITE_IMPLEMENTATION
#endif // VODUS_RELEASE

#include "./stb_image.h"
#include "./stb_image_resize.h"
#include "./stb_image_write.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

#include "./aids.hpp"
using namespace aids;

#ifdef VODUS_SSE
#include <tmmintrin.h>
#include <smmintrin.h>
#endif // VODUS_SSE

// PLEASE READ THIS --> https://en.wikipedia.org/wiki/Single_Compilation_Unit
#include "./vodus_error.cpp"
#include "./vodus_queue.cpp"
#include "./vodus_image32.cpp"
#include "./vodus_video_params.cpp"
#include "./vodus_emotes.cpp"
#include "./vodus_message.cpp"
#include "./vodus_encoder.cpp"
#include "./vodus_main.cpp"
