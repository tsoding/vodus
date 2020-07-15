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
                // TODO(#6): how do we mix alphas?
            }
        }
    }
}

// TODO(#24): resizing version of slap_image32_onto_image32 requires some sort of interpolation
void slap_image32_onto_image32(Image32 dst, Image32 src,
                               int x0, int y0)
{
    size_t x1 = std::min(x0 + src.width, dst.width);
    size_t y1 = std::min(y0 + src.height, dst.height);

    for (size_t y = y0; y < y1; ++y) {
        for (size_t x = x0; x < x1; ++x) {
            assert(x >= 0);
            assert(y >= 0);
            assert(x < dst.width);
            assert(y < dst.height);

            assert(x - x0 >= 0);
            assert(y - y0 >= 0);
            assert(x - x0 < src.width);
            assert(y - y0 < src.height);

            dst.pixels[y * dst.width + x] =
                mix_pixels(
                    dst.pixels[y * dst.width + x],
                    src.pixels[(y - y0) * src.width + (x - x0)]);
        }
    }
}

void slap_savedimage_onto_image32(Image32 dest,
                                  SavedImage *src,
                                  ColorMapObject *SColorMap,
                                  GraphicsControlBlock gcb,
                                  int x, int y)
{
    assert(SColorMap);
    assert(SColorMap->BitsPerPixel <= 8);
    assert(!SColorMap->SortFlag);
    assert(src);
    assert(src->ImageDesc.Left == 0);
    assert(src->ImageDesc.Top == 0);

    for (size_t row = 0; ((int) row < src->ImageDesc.Height); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; ((int) col < src->ImageDesc.Width); ++col) {
                if (col + x < dest.width) {
                    auto index = src->RasterBits[row * src->ImageDesc.Width + col];
                    if (index != gcb.TransparentColor) {
                        auto pixel = SColorMap->Colors[index];
                        dest.pixels[(row + y) * dest.width + col + x].r = pixel.Red;
                        dest.pixels[(row + y) * dest.width + col + x].g = pixel.Green;
                        dest.pixels[(row + y) * dest.width + col + x].b = pixel.Blue;
                    }
                }
            }
        }
    }
}

void slap_savedimage_onto_image32(Image32 dest,
                                  SavedImage *src,
                                  ColorMapObject *SColorMap,
                                  GraphicsControlBlock gcb,
                                  int x, int y,
                                  int target_width, int target_height)
{
    assert(SColorMap);
    assert(SColorMap->BitsPerPixel <= 8);
    assert(!SColorMap->SortFlag);
    assert(src);
    assert(src->ImageDesc.Left == 0);
    assert(src->ImageDesc.Top == 0);

    for (size_t row = 0; ((int) row < target_height); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; ((int) col < target_width); ++col) {
                if (col + x < dest.width) {
                    int src_row = floorf((float) row / target_height * src->ImageDesc.Height);
                    int src_col = floorf((float) col / target_width * src->ImageDesc.Width);

                    auto index = src->RasterBits[src_row * src->ImageDesc.Width + src_col];
                    if (index != gcb.TransparentColor) {
                        auto pixel = SColorMap->Colors[index];
                        dest.pixels[(row + y) * dest.width + col + x].r = pixel.Red;
                        dest.pixels[(row + y) * dest.width + col + x].g = pixel.Green;
                        dest.pixels[(row + y) * dest.width + col + x].b = pixel.Blue;
                    }
                }
            }
        }
    }
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
    for (size_t i = 0; i < text.count; ++i) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, text.data[i]);

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

void slap_text_onto_image32_wrapped(Image32 surface,
                                    FT_Face face,
                                    String_View text,
                                    Pixel32 color,
                                    int *pen_x, int *pen_y,
                                    size_t font_size)
{
    int copy_x = *pen_x;
    int copy_y = *pen_y;

    advance_pen_for_text(face, text, &copy_x, &copy_y);
    if (copy_x >= (int)surface.width) {
        *pen_x = 0;
        *pen_y += font_size;
    }

    slap_text_onto_image32(surface, face, text, color, pen_x, pen_y);
}

void slap_text_onto_image32_wrapped(Image32 surface,
                                    FT_Face face,
                                    const char *cstr,
                                    Pixel32 color,
                                    int *pen_x, int *pen_y,
                                    size_t font_size)
{
    slap_text_onto_image32_wrapped(surface,
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
