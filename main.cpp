#include <cassert>
#include <cstdio>
#include <cstdint>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <gif_lib.h>

struct Pixel32
{
    uint8_t r, g, b, a;
};

struct Image32
{
    size_t width, height;
    Pixel32 *pixels;
};

int save_image32_as_ppm(Image32 *image, const char *filename)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    fprintf(f,
            "P6\n"
            "%d %d\n"
            "255\n",
            image->width,
            image->height);

    for (int row = 0; row < image->height; ++row) {
        for (int col = 0; col < image->width; ++col) {
            Pixel32 p = *(image->pixels + image->width * row + col);
            fputc(p.r, f);
            fputc(p.g, f);
            fputc(p.b, f);
        }
    }

    fclose(f);

    return 0;
}

void slap_onto_image32(Image32 *dest, FT_Bitmap *src, int x, int y)
{
    for (int row = 0;
         (row < src->rows) && (row + y < dest->height);
         ++row) {
        for (int col = 0;
             (col < src->width) && (col + x < dest->width);
             ++col) {
            dest->pixels[(row + y) * dest->width + col + x].r =
                src->buffer[row * src->pitch + col];
            dest->pixels[(row + y) * dest->width + col + x].g = 0;
            dest->pixels[(row + y) * dest->width + col + x].b = 0;
        }
    }
}

// TODO: slap_onto_image32 for libgif SavedImage does not support transparency
void slap_onto_image32(Image32 *dest,
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

    for (int row = 0;
         (row < src->ImageDesc.Height) && (row + y < dest->height);
         ++row) {
        for (int col = 0;
             (col < src->ImageDesc.Width) && (col + x < dest->width);
             ++col) {
            auto pixel =
                SColorMap->Colors[src->RasterBits[row * src->ImageDesc.Width + col]];
            dest->pixels[(row + y) * dest->width + col + x].r = pixel.Red;
            dest->pixels[(row + y) * dest->width + col + x].g = pixel.Green;
            dest->pixels[(row + y) * dest->width + col + x].b = pixel.Blue;
        }
    }
}

int save_bitmap_as_ppm(FT_Bitmap *bitmap,
                       const char *filename)
{
    assert(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);

    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    fprintf(f,
            "P6\n"
            "%d %d\n"
            "255\n",
            bitmap->width,
            bitmap->rows);

    for (int row = 0; row < bitmap->rows; ++row) {
        for (int col = 0; col < bitmap->width; ++col) {
            char x = *(bitmap->buffer + bitmap->pitch * row + col);
            fputc(x, f);
            fputc(x, f);
            fputc(x, f);
        }
    }

    fclose(f);
    return 0;
}

void slap_bitmap_onto_bitmap(FT_Bitmap *dest,
                             FT_Bitmap *src,
                             int x, int y)
{
    assert(dest);
    assert(dest->pixel_mode == FT_PIXEL_MODE_GRAY);
    assert(src);
    assert(src->pixel_mode == FT_PIXEL_MODE_GRAY);

    for (int row = 0;
         (row < src->rows) && (row + y < dest->rows);
         ++row) {
        for (int col = 0;
             (col < src->width) && (col + x < dest->width);
             ++col) {
            dest->buffer[(row + y) * dest->pitch + col + x] =
                src->buffer[row * src->pitch + col];
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: ./vodus <text> <gif_image> [font]\n");
        exit(1);
    }

    const char *text = argv[1];
    const char *gif_filepath = argv[2];
    const char *face_file_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

    if (argc >= 4) {
        face_file_path = argv[3];
    }

    FT_Library library;
    auto error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "Could not initialize FreeType2\n");
        exit(1);
    }

    FT_Face face;
    error = FT_New_Face(library,
                        face_file_path,
                        0,
                        &face);
    if (error == FT_Err_Unknown_File_Format) {
        fprintf(stderr,
                "%s appears to have an unsuppoted format\n",
                face_file_path);
        exit(1);
    } else if (error) {
        fprintf(stderr,
                "%s could not be opened\n",
                face_file_path);
        exit(1);
    }

    printf("Loaded %s\n", face_file_path);
    printf("\tnum_glyphs = %ld\n", face->num_glyphs);

    error = FT_Set_Pixel_Sizes(face, 0, 64);
    if (error) {
        fprintf(stderr, "Could not set font size in pixels\n");
        exit(1);
    }

    auto gif_file = DGifOpenFileName(gif_filepath, &error);
    if (error) {
        fprintf(stderr, "Could not read gif file: %s", gif_filepath);
        exit(1);
    }
    assert(error == 0);
    DGifSlurp(gif_file);

    Image32 surface;
    surface.width = 800;
    surface.height = 600;
    surface.pixels = (Pixel32 *) malloc(surface.width * surface.height * sizeof(Pixel32));
    assert(surface.pixels);

    const size_t text_count = strlen(text);
    int pen_x = 0, pen_y = 100;
    for (int i = 0; i < text_count; ++i) {
        FT_UInt glyph_index = FT_Get_Char_Index(face, text[i]);

        error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
        assert(!error);

        error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        assert(!error);

        slap_onto_image32(&surface, &face->glyph->bitmap,
                          pen_x + face->glyph->bitmap_left,
                          pen_y - face->glyph->bitmap_top);

        pen_x += face->glyph->advance.x >> 6;
    }

    assert(gif_file->ImageCount > 0);
    slap_onto_image32(&surface,
                      &gif_file->SavedImages[0],
                      gif_file->SColorMap,
                      0, 0);

    save_image32_as_ppm(&surface, "output.ppm");

    return 0;
}
