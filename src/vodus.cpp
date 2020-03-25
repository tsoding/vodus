#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include <functional>
#include <algorithm>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <gif_lib.h>
#include <png.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

const size_t VODUS_FPS = 30;
const float VODUS_DELTA_TIME_SEC = 1.0f / VODUS_FPS;
const size_t VODUS_WIDTH = 1028;
const size_t VODUS_HEIGHT = 768;
const size_t VODUS_FONT_SIZE = 64;
const size_t VODUS_MESSAGES_CAPACITY = 1024;

template <typename F>
struct Defer
{
    Defer(F f): f(f) {}
    ~Defer() { f(); }
    F f;
};

#define CONCAT0(a, b) a##b
#define CONCAT(a, b) CONCAT0(a, b)
#define defer(body) Defer CONCAT(defer, __LINE__)([&]() { body; })

// PLEASE READ THIS --> https://en.wikipedia.org/wiki/Single_Compilation_Unit
#include "./vodus_print.cpp"
#include "./vodus_string.cpp"
#include "./vodus_image32.cpp"
#include "./vodus_bttv.cpp"
#include "./vodus_main.cpp"
