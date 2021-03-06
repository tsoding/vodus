#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <ctime>
#include <climits>

#include <algorithm>

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

#define GLFW_INCLUDE_GLCOREARB
#define GL_GLEXT_PROTOTYPES
#include <GLFW/glfw3.h>

// PLEASE READ THIS --> https://en.wikipedia.org/wiki/Single_Compilation_Unit
#include "./vodus_error.cpp"
#include "./vodus_queue.cpp"
#include "./vodus_image32.cpp"
#include "./vodus_video_params.cpp"
#include "./vodus_emotes.cpp"
#include "./vodus_message.cpp"
#include "./vodus_encoder.cpp"
#include "./vodus_main.cpp"
