#include <cassert>
#include <cstdio>

#include <ft2build.h>
#include FT_FREETYPE_H

#define FACE_FILE_PATH "/usr/share/fonts/opentype/comic-neue/ComicNeue_Oblique.otf"

int save_bitmap_as_ppm(FT_Bitmap *bitmap,
                       char prefix_char)
{
    assert(bitmap->pixel_mode == FT_PIXEL_MODE_GRAY);

    char filename[10];
    snprintf(filename, 10, "%c.ppm", prefix_char);

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

int main(int argc, char *argv[])
{
    FT_Library library;
    auto error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "Could not initialize FreeType2\n");
        exit(1);
    }

    FT_Face face;
    error = FT_New_Face(library,
                        FACE_FILE_PATH,
                        0,
                        &face);
    if (error == FT_Err_Unknown_File_Format) {
        fprintf(stderr,
                "%s appears to have an unsuppoted format\n",
                FACE_FILE_PATH);
        exit(1);
    } else if (error) {
        fprintf(stderr,
                "%s could not be opened\n",
                FACE_FILE_PATH);
        exit(1);
    }

    printf("Loaded %s\n", FACE_FILE_PATH);
    printf("\tnum_glyphs = %ld\n", face->num_glyphs);

    error = FT_Set_Pixel_Sizes(face, 0, 64);
    if (error) {
        fprintf(stderr, "Could not set font size in pixels\n");
        exit(1);
    }

    const char *text = "abcd";
    while (*text != '\0') {
        auto glyph_index = FT_Get_Char_Index(face, *text);
        error = FT_Load_Glyph(
            face,
            glyph_index,
            0);

        if (error) {
            fprintf(stderr, "Could not load glyph for %c\n", *text);
            exit(1);
        }

        error = FT_Render_Glyph(face->glyph,
                                FT_RENDER_MODE_NORMAL);
        if (error) {
            fprintf(stderr, "Could not render glyph for %c\n", *text);
            exit(1);
        }

        printf("\t'%c'\n", *text);
        printf("\t\tglyph_index: %ld\n", glyph_index);
        printf("\t\trows: %ld\n", face->glyph->bitmap.rows);
        printf("\t\twidth: %ld\n", face->glyph->bitmap.width);
        printf("\t\tpixel_mode: %ld\n", face->glyph->bitmap.pixel_mode);

        save_bitmap_as_ppm(&face->glyph->bitmap, *text);

        text++;
    }

    return 0;
}
