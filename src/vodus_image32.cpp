// TODO(#53): store all of the loaded images in Pixel for performance reason
struct Pixel
{
    float r, g, b, a;
};

struct Pixel32
{
    uint8_t r, g, b, a;
};

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

Pixel pixel32_to_pixel(Pixel32 pixel)
{
    return Pixel {
        ((float) pixel.r) / 255.0f,
        ((float) pixel.g) / 255.0f,
        ((float) pixel.b) / 255.0f,
        ((float) pixel.a) / 255.0f
    };
}

Pixel32 pixel_to_pixel32(Pixel pixel)
{
    return Pixel32 {
        (uint8_t) floorf(pixel.r * 255.0f),
        (uint8_t) floorf(pixel.g * 255.0f),
        (uint8_t) floorf(pixel.b * 255.0f),
        (uint8_t) floorf(pixel.a * 255.0f)
    };
}

Pixel32 mix_pixels(Pixel32 b32, Pixel32 a32)
{
    const auto a = pixel32_to_pixel(a32);
    const auto b = pixel32_to_pixel(b32);

    Pixel r = {};
    r.a = a.a + b.a * (1.0f - a.a);
    r.r = (a.r * a.a + b.r * b.a * (1.0f - a.a)) / r.a;
    r.g = (a.g * a.a + b.g * b.a * (1.0f - a.a)) / r.a;
    r.b = (a.b * a.a + b.b * b.a * (1.0f - a.a)) / r.a;

    return pixel_to_pixel32(r);
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
                    float a = src->buffer[row * src->pitch + col] / 255.0f;
                    dest.pixels[(row + y) * dest.width + col + x].r =
                        a * color.r + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].r;
                    dest.pixels[(row + y) * dest.width + col + x].g =
                        a * color.g + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].g;
                    dest.pixels[(row + y) * dest.width + col + x].b =
                        a * color.b + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].b;
                    dest.pixels[(row + y) * dest.width + col + x].a =
                        a * color.a + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].a;
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
                                    int *pen_x, int *pen_y)
{
    int copy_x = *pen_x;
    int copy_y = *pen_y;

    advance_pen_for_text(face, text, &copy_x, &copy_y);
    if (copy_x >= (int)surface.width) {
        *pen_x = 0;
        // @word-wrap-face
        *pen_y += VODUS_FONT_SIZE;
    }

    slap_text_onto_image32(surface, face, text, color, pen_x, pen_y);
}

void slap_text_onto_image32_wrapped(Image32 surface,
                                    FT_Face face,
                                    const char *cstr,
                                    Pixel32 color,
                                    int *pen_x, int *pen_y)
{
    slap_text_onto_image32_wrapped(surface,
                                   face,
                                   cstr_as_string_view(cstr),
                                   color,
                                   pen_x, pen_y);
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
