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

Pixel32 mix_pixels(Pixel32 b32, Pixel32 a32)
{
    const float a32_alpha = a32.a / 255.0;
    const float b32_alpha = b32.a / 255.0;
    const float r_alpha = a32_alpha + b32_alpha * (1.0f - a32_alpha);

    Pixel32 r = {};

    r.r = (uint8_t) ((a32.r * a32_alpha + b32.r * b32_alpha * (1.0f - a32_alpha)) / r_alpha);
    r.g = (uint8_t) ((a32.g * a32_alpha + b32.g * b32_alpha * (1.0f - a32_alpha)) / r_alpha);
    r.b = (uint8_t) ((a32.b * a32_alpha + b32.b * b32_alpha * (1.0f - a32_alpha)) / r_alpha);
    r.a = (uint8_t) (r_alpha * 255.0);

    return r;
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
void slap_image32_onto_image32(Image32 dest, Image32 src,
                               int x, int y,
                               size_t target_width,
                               size_t target_height)
{
    for (size_t row = 0; (row < target_height); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; (col < target_width); ++col) {
                if (col + x < dest.width) {
                    const int src_row = floorf((float) row / target_height * src.height);
                    const int src_col = floorf((float) col / target_width * src.width);
                    const auto dest_pixel = dest.pixels[(row + y) * dest.width + col + x];
                    const auto src_pixel = src.pixels[src_row * src.width + src_col];
                    dest.pixels[(row + y) * dest.width + col + x] =
                        mix_pixels(dest_pixel, src_pixel);
                }
            }
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

Image32 load_image32_from_png(const char *filepath)
{
    Image32 result = {};
    int w, h, n;
    result.pixels = (Pixel32*) stbi_load(filepath, &w, &h, &n, 4);
    result.width = w;
    result.height = h;
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
