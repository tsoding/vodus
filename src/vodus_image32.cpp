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

void slap_onto_image32(Image32 dest, FT_Bitmap *src, Pixel32 color, int x, int y)
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

void slap_onto_image32(Image32 dest, Image32 src, int x, int y)
{
    for (size_t row = 0; (row < src.height); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; (col < src.width); ++col) {
                if (col + x < dest.width) {
                    dest.pixels[(row + y) * dest.width + col + x] =
                        src.pixels[row * src.height + col];
                }
            }
        }
    }
}

// TODO(#9): slap_onto_image32 for libgif SavedImage does not support transparency
void slap_onto_image32(Image32 dest,
                       SavedImage *src,
                       ColorMapObject *SColorMap,
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
                    auto pixel =
                        SColorMap->Colors[src->RasterBits[row * src->ImageDesc.Width + col]];
                    dest.pixels[(row + y) * dest.width + col + x].r = pixel.Red;
                    dest.pixels[(row + y) * dest.width + col + x].g = pixel.Green;
                    dest.pixels[(row + y) * dest.width + col + x].b = pixel.Blue;
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

void slap_text_onto_image32(Image32 surface,
                            FT_Face face,
                            const char *text,
                            Pixel32 color,
                            int x, int y)
{
    const size_t text_count = strlen(text);
    int pen_x = x, pen_y = y;
    for (size_t i = 0; i < text_count; ++i) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, text[i]);

        auto error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        assert(!error);

        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        assert(!error);

        slap_onto_image32(surface, &face->glyph->bitmap,
                          color,
                          pen_x + face->glyph->bitmap_left,
                          pen_y - face->glyph->bitmap_top);

        pen_x += face->glyph->advance.x >> 6;
    }
}
