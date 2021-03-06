struct Pixel32
{
    uint8_t r, g, b, a;
};

void print1(FILE *stream, Pixel32 pixel)
{
#define PRINT_BYTE(x)                               \
    do {                                            \
        const auto __x = (x);                       \
        const auto __u = __x / 0x10;                \
        const auto __l = __x % 0x10;                \
        print(stream,                               \
              (char) (__u + (__u <= 9 ? '0' : 'a' - 10)),         \
              (char) (__l + (__l <= 9 ? '0' : 'a' - 10)));        \
    } while (0)

    PRINT_BYTE(pixel.r);
    PRINT_BYTE(pixel.g);
    PRINT_BYTE(pixel.b);
    PRINT_BYTE(pixel.a);
}

struct Image32
{
    size_t width;
    size_t height;
    Pixel32 *pixels;

    bool is_null() const
    {
        return pixels == nullptr;
    }
};

struct Animat32
{
    Image32 *frames;
    int *frame_delays;
    size_t count;
};

#ifdef VODUS_SSE
// NOTE: Stolen from https://stackoverflow.com/a/53707227
void mix_pixels_sse(Pixel32 *src, Pixel32 *dst, Pixel32 *c)
{
    const __m128i _swap_mask =
        _mm_set_epi8(7,  6,   5,  4,
                     3,  2,   1,  0,
                     15, 14, 13, 12,
                     11, 10,  9,  8
            );

    const __m128i _aa =
        _mm_set_epi8( 15,15,15,15,
                      11,11,11,11,
                      7,7,7,7,
                      3,3,3,3 );

    const __m128i _mask1 = _mm_set_epi16(-1,0,0,0, -1,0,0,0);
    const __m128i _mask2 = _mm_set_epi16(0,-1,-1,-1, 0,-1,-1,-1);
    const __m128i _v1 = _mm_set1_epi16( 1 );

    __m128i _src = _mm_loadu_si128((__m128i*)src);
    __m128i _src_a = _mm_shuffle_epi8(_src, _aa);

    __m128i _dst = _mm_loadu_si128((__m128i*)dst);
    __m128i _dst_a = _mm_shuffle_epi8(_dst, _aa);
    __m128i _one_minus_src_a = _mm_subs_epu8(
        _mm_set1_epi8(-1), _src_a);

    __m128i _out = {};
    {
        __m128i _s_a = _mm_cvtepu8_epi16( _src_a );
        __m128i _s = _mm_cvtepu8_epi16( _src );
        __m128i _d = _mm_cvtepu8_epi16( _dst );
        __m128i _d_a = _mm_cvtepu8_epi16( _one_minus_src_a );
        _out = _mm_adds_epu16(
            _mm_mullo_epi16(_s, _s_a),
            _mm_mullo_epi16(_d, _d_a));
        _out = _mm_srli_epi16(
            _mm_adds_epu16(
                _mm_adds_epu16( _v1, _out ),
                _mm_srli_epi16( _out, 8 ) ), 8 );
        _out = _mm_or_si128(
            _mm_and_si128(_out,_mask2),
            _mm_and_si128(
                _mm_adds_epu16(
                    _s_a,
                    _mm_cvtepu8_epi16(_dst_a)), _mask1));
    }

    // compute _out2 using high quadword of of the _src and _dst
    //...
    __m128i _out2 = {};
    {
        __m128i _s_a = _mm_cvtepu8_epi16(_mm_shuffle_epi8(_src_a, _swap_mask));
        __m128i _s = _mm_cvtepu8_epi16(_mm_shuffle_epi8(_src, _swap_mask));
        __m128i _d = _mm_cvtepu8_epi16(_mm_shuffle_epi8(_dst, _swap_mask));
        __m128i _d_a = _mm_cvtepu8_epi16(_mm_shuffle_epi8(_one_minus_src_a, _swap_mask));
        _out2 = _mm_adds_epu16(
            _mm_mullo_epi16(_s, _s_a),
            _mm_mullo_epi16(_d, _d_a));
        _out2 = _mm_srli_epi16(
            _mm_adds_epu16(
                _mm_adds_epu16( _v1, _out2 ),
                _mm_srli_epi16( _out2, 8 ) ), 8 );
        _out2 = _mm_or_si128(
            _mm_and_si128(_out2,_mask2),
            _mm_and_si128(
                _mm_adds_epu16(
                    _s_a,
                    _mm_cvtepu8_epi16(_dst_a)), _mask1));
    }

    __m128i _ret = _mm_packus_epi16( _out, _out2 );

    _mm_storeu_si128( (__m128i_u*) c, _ret );
}
#endif // VODUS_SSE

Pixel32 mix_pixels(Pixel32 dst, Pixel32 src)
{
    uint8_t rev_src_a = 255 - src.a;
    Pixel32 result;
    result.r = ((uint16_t) src.r * (uint16_t) src.a + (uint16_t) dst.r * rev_src_a) >> 8;
    result.g = ((uint16_t) src.g * (uint16_t) src.a + (uint16_t) dst.g * rev_src_a) >> 8;
    result.b = ((uint16_t) src.b * (uint16_t) src.a + (uint16_t) dst.b * rev_src_a) >> 8;
    result.a = dst.a;
    return result;
}

void fill_image32_with_color(Image32 image, Pixel32 color)
{
    size_t n = image.width * image.height;
    for (size_t i = 0; i < n; ++i)
        image.pixels[i] = color;
}

void slap_ftbitmap_onto_image32(Image32 dest, FT_Bitmap *src, Pixel32 color, int x, int y)
{
    assert(src->pixel_mode == FT_PIXEL_MODE_GRAY);
    assert(src->num_grays == 256);

    for (size_t row = 0; (row < src->rows); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; (col < src->width); ++col) {
                if (col + x < dest.width) {
                    color.a = src->buffer[row * src->pitch + col];
                    dest.pixels[(row + y) * dest.width + col + x] =
                        mix_pixels(
                            dest.pixels[(row + y) * dest.width + col + x],
                            color);
                }
            }
        }
    }
}

// TODO(#24): resizing version of slap_image32_onto_image32 requires some sort of interpolation
void slap_image32_onto_image32(Image32 dst, Image32 src,
                               int x0, int y0)
{
#ifdef VODUS_SSE
    const size_t SIMD_PIXEL_PACK_SIZE = 4;
#else
    const size_t SIMD_PIXEL_PACK_SIZE = 1;
#endif // VODUS_SSE

    size_t x1 = std::min(x0 + src.width, dst.width);
    size_t y1 = std::min(y0 + src.height, dst.height);

    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = x0;
             x + SIMD_PIXEL_PACK_SIZE < x1;
             x += SIMD_PIXEL_PACK_SIZE)
        {
            assert(x >= 0);
            assert(y >= 0);
            assert(x < dst.width);
            assert(y < dst.height);

            assert(x - x0 >= 0);
            assert(y - y0 >= 0);
            assert(x - x0 < src.width);
            assert(y - y0 < src.height);

#ifdef VODUS_SSE
            mix_pixels_sse(
                &src.pixels[(y - y0) * src.width + (x - x0)],
                &dst.pixels[y * dst.width + x],
                &dst.pixels[y * dst.width + x]);
#else
            dst.pixels[y * dst.width + x] =
                mix_pixels(
                    dst.pixels[y * dst.width + x],
                    src.pixels[(y - y0) * src.width + (x - x0)]);
#endif // VODUS_SSE
        }
    }
}

Maybe<Pixel32> hexstr_as_pixel32(String_View hexstr)
{
    if (hexstr.count != 8) return {};

    Pixel32 result = {};
    unwrap_into(result.r, hexstr.subview(0, 2).from_hex<uint8_t>());
    unwrap_into(result.g, hexstr.subview(2, 2).from_hex<uint8_t>());
    unwrap_into(result.b, hexstr.subview(4, 2).from_hex<uint8_t>());
    unwrap_into(result.a, hexstr.subview(6, 2).from_hex<uint8_t>());
    return {true, result};
}

void slap_savedimage_to_image32(Image32 vscreen,
                                ColorMapObject *screen_color_map,
                                SavedImage *saved_image,
                                GraphicsControlBlock gcb)
{
    assert(screen_color_map);
    assert(screen_color_map->BitsPerPixel <= 8);
    assert(!screen_color_map->SortFlag);

    SavedImage *src = saved_image;

    assert(src->ImageDesc.Left >= 0);
    assert(src->ImageDesc.Top >= 0);

    for (size_t y = 0; (int) y < src->ImageDesc.Height; ++y) {
        for (size_t x = 0; (int) x < src->ImageDesc.Width; ++x) {
            auto src_color_index = src->RasterBits[y * src->ImageDesc.Width + x];
            GifColorType pixel = {};
            if (src->ImageDesc.ColorMap) {
                pixel = src->ImageDesc.ColorMap->Colors[src_color_index];
            } else {
                pixel = screen_color_map->Colors[src_color_index];
            }
            auto dst_pixel_index = (y + src->ImageDesc.Top) * vscreen.width + (x + src->ImageDesc.Left);
            if (src_color_index != gcb.TransparentColor) {
                vscreen.pixels[dst_pixel_index].r = pixel.Red;
                vscreen.pixels[dst_pixel_index].g = pixel.Green;
                vscreen.pixels[dst_pixel_index].b = pixel.Blue;
                vscreen.pixels[dst_pixel_index].a = 255;
            }
        }
    }
}

void save_image32_as_png(Image32 image, const char *filepath)
{
    int stbi_ret = stbi_write_png(
        filepath,
        image.width,
        image.height,
        4,
        image.pixels,
        image.width * sizeof(image.pixels[0]));
    assert(stbi_ret);
}

Animat32 load_animat32_from_gif(const char *filepath, size_t size)
{
    int error = 0;
    GifFileType *gif_file = DGifOpenFileName(filepath, &error);
    if (error) {
        println(stderr, "[ERROR] Could not open gif file: ", filepath);
        abort();
    }

    if (DGifSlurp(gif_file) == GIF_ERROR) {
        println(stderr, "[ERROR] Could not read gif file: ", filepath);
        abort();
    }

    GraphicsControlBlock gcb = {};
    Image32 vscreen = {};
    vscreen.width = gif_file->SWidth;
    vscreen.height = gif_file->SHeight;
    vscreen.pixels = new Pixel32[vscreen.width * vscreen.height];
    memset(vscreen.pixels, 0, sizeof(Pixel32) * vscreen.width * vscreen.height);
    defer(delete[] vscreen.pixels);

    Image32 prev_vscreen = {};
    prev_vscreen.width = gif_file->SWidth;
    prev_vscreen.height = gif_file->SHeight;
    prev_vscreen.pixels = new Pixel32[prev_vscreen.width * prev_vscreen.height];
    memset(prev_vscreen.pixels, 0, sizeof(Pixel32) * prev_vscreen.width * prev_vscreen.height);
    defer(delete[] prev_vscreen.pixels);

    Animat32 result = {};
    result.count = gif_file->ImageCount;
    result.frames = new Image32[result.count];
    result.frame_delays = new int[result.count];

    const float emote_ratio = (float) vscreen.width / (float) vscreen.height;
    for (size_t index = 0; index < result.count; ++index) {
        memcpy(prev_vscreen.pixels, vscreen.pixels, sizeof(Pixel32) * vscreen.width * vscreen.height);

        int ok = DGifSavedExtensionToGCB(gif_file, index, &gcb);

        if (!ok) {
            println(stderr, "[ERROR] Could not retrieve Graphics Control Block from `",
                    filepath, "` on index ", index);
            abort();
        }

        result.frame_delays[index] = gcb.DelayTime;

        slap_savedimage_to_image32(vscreen,
                                   gif_file->SColorMap,
                                   &gif_file->SavedImages[index],
                                   gcb);

        result.frames[index].height = (int) size;
        result.frames[index].width = (int) floorf(result.frames[index].height * emote_ratio);
        result.frames[index].pixels = new Pixel32[result.frames[index].width * result.frames[index].height];

        stbir_resize_uint8((unsigned char *) vscreen.pixels, vscreen.width, vscreen.height, 0,
                           (unsigned char *) result.frames[index].pixels,
                           result.frames[index].width,
                           result.frames[index].height,
                           0,
                           4);

        switch (gcb.DisposalMode) {
        case DISPOSE_DO_NOT:
            break;
        case DISPOSE_BACKGROUND:
            // NOTE: This is a little bit of cheating. I think we are
            // supposed to fill up the virtual screen with
            // gif_file->SBackGroundColor. But the problem is that
            // it's unclear how to handle transparency in that case,
            // because transparency is defined per frame in
            // gif_file->SavedImages[index]. So in some frames
            // gif_file->SBackGroundColor is transparent, in some
            // frames it is not. So we decided to ignore
            // gif_file->SBackGroundColor and fill up the virtual
            // screen with transparency always.
            //
            // I have a feeling that majority of other software do the
            // same thing as well. (we probably wanna check that).
            memset(vscreen.pixels, 0, sizeof(Pixel32) * vscreen.width * vscreen.height);
            break;
        case DISPOSE_PREVIOUS:
            memcpy(vscreen.pixels, prev_vscreen.pixels, sizeof(Pixel32) * vscreen.width * vscreen.height);
            break;
        default:
            assert(0 && "Unsupported gif frame disposal mode");
        }

    }

    DGifCloseFile(gif_file, &error);
    if (error) {
        println(stderr, "[ERROR] Could not close gif file: ", filepath);
        abort();
    }

    return result;
}

Image32 load_image32_from_png(const char *filepath, size_t size)
{
    Image32 result = {};
    int w, h, n;

    unsigned char *origin = stbi_load(filepath, &w, &h, &n, 4);
    defer(stbi_image_free(origin));

    const float emote_ratio = (float) w / (float) h;
    result.height = (int) size;
    result.width = (int) floorf(result.height * emote_ratio);
    result.pixels = new Pixel32[result.width * result.height];

    stbir_resize_uint8(origin, w, h, 0,
                       (unsigned char *) result.pixels,
                       result.width, result.height, 0,
                       4);

    return result;
}

void advance_pen_for_text(FT_Face face, String_View text, int *pen_x, int *pen_y)
{
    for (size_t i = 0; i < text.count; ++i) {
        auto error = FT_Load_Glyph(
            face,
            FT_Get_Char_Index(face, text.data[i]),
            FT_LOAD_DEFAULT);
        assert(!error);

        *pen_x += face->glyph->advance.x >> 6;
    }
}

void slap_text_onto_image32(Image32 surface,
                            FT_Face face,
                            String_View text,
                            Pixel32 color,
                            int *pen_x, int *pen_y)
{
    String_View iter = text;

    while (iter.count > 0) {
        size_t size = 0;
        auto code = utf8_get_code(iter, &size);
        if (!code.has_value) {
            println(stderr, "`", text, "` is not a correct UTF-8 sequence");
            abort();
        }
        FT_UInt glyph_index = FT_Get_Char_Index(face, code.unwrap);
        iter.chop(size);

        auto error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        assert(!error);

        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        assert(!error);

        slap_ftbitmap_onto_image32(surface, &face->glyph->bitmap,
                                   color,
                                   *pen_x + face->glyph->bitmap_left,
                                   *pen_y - face->glyph->bitmap_top);

        *pen_x += face->glyph->advance.x >> 6;
    }
}

/// returns true if wrapped around
bool slap_text_onto_image32_wrapped(Image32 surface,
                                    FT_Face face,
                                    String_View text,
                                    Pixel32 color,
                                    int *pen_x, int *pen_y,
                                    size_t font_size)
{
    bool wrapped = false;

    int copy_x = *pen_x;
    int copy_y = *pen_y;

    advance_pen_for_text(face, text, &copy_x, &copy_y);
    if (copy_x >= (int)surface.width) {
        *pen_x = 0;
        *pen_y += font_size;
        wrapped = true;
    }

    slap_text_onto_image32(surface, face, text, color, pen_x, pen_y);
    return wrapped;
}

/// returns true if wrapped around
bool slap_text_onto_image32_wrapped(Image32 surface,
                                    FT_Face face,
                                    const char *cstr,
                                    Pixel32 color,
                                    int *pen_x, int *pen_y,
                                    size_t font_size)
{
    return slap_text_onto_image32_wrapped(surface,
                                          face,
                                          cstr_as_string_view(cstr),
                                          color,
                                          pen_x, pen_y,
                                          font_size);
}

void slap_text_onto_image32(Image32 surface,
                            FT_Face face,
                            const char *text,
                            Pixel32 color,
                            int *pen_x, int *pen_y)
{
    slap_text_onto_image32(surface,
                           face,
                           cstr_as_string_view(text),
                           color,
                           pen_x, pen_y);
}
