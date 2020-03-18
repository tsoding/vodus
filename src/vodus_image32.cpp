struct Pixel32
{
    uint8_t r, g, b, a;
};

struct Image32
{
    size_t width;
    size_t height;
    Pixel32 *pixels;
};

int save_image32_as_png(Image32 image, const char *filename)
{
    png_image pimage = {0};
    pimage.width = image.width;
    pimage.height = image.height;
    pimage.version = PNG_IMAGE_VERSION;
    pimage.format = PNG_FORMAT_RGBA;

    return png_image_write_to_file(&pimage,
                                   filename, 0, image.pixels,
                                   0, nullptr);
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
                }
                // TODO(#6): how do we mix alphas?
            }
        }
    }
}

// TODO(#22): slap_image32_onto_image32 does not support alpha blending
void slap_image32_onto_image32(Image32 dest, Image32 src, int x, int y)
{
    for (size_t row = 0; (row < src.height); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; (col < src.width); ++col) {
                if (col + x < dest.width) {
                    dest.pixels[(row + y) * dest.width + col + x] =
                        src.pixels[row * src.width + col];
                }
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
                    int src_row = floorf((float) row / target_height * src.height);
                    int src_col = floorf((float) col / target_width * src.width);
                    dest.pixels[(row + y) * dest.width + col + x] =
                        src.pixels[src_row * src.width + src_col];
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
    assert(SColorMap->BitsPerPixel == 8);
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
    assert(SColorMap->BitsPerPixel == 8);
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
    png_image image = {0};
    image.version = PNG_IMAGE_VERSION;
    png_image_begin_read_from_file(&image, filepath);

    if (image.warning_or_error) {
        fprintf(stderr, "Could not load file %s: %s\n", filepath, image.message);
        exit(1);
    }

    image.format = PNG_FORMAT_RGBA;
    Pixel32 *buffer = new Pixel32[image.width * image.height];
    png_image_finish_read(&image, NULL, buffer, 0, NULL);

    if (image.warning_or_error) {
        fprintf(stderr, "Could not load file %s: %s\n", filepath, image.message);
        exit(1);
    }

    Image32 result = {
        .width = image.width,
        .height = image.height,
        .pixels = buffer,
    };

    png_image_free(&image);

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
        // TODO: the size of the font in slap_text_onto_image32_wrapped should be taken from the face itself
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
