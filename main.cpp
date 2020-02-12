#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cmath>

#include <atomic>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <gif_lib.h>

#include <png.h>

#include <pthread.h>

template <typename F>
struct Defer
{
    Defer(F f):
        f(f)
    {}

    ~Defer()
    {
        f();
    }

    F f;
};

#define CONCAT0(a, b) a##b
#define CONCAT(a, b) CONCAT0(a, b)
#define defer(body) Defer CONCAT(defer, __LINE__)([&]() { body; })

struct Pixel32
{
    uint8_t r, g, b, a;
};

struct Image32
{
    size_t width, height;
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

int save_image32_as_ppm(Image32 image, const char *filename)
{
    FILE *f = fopen(filename, "wb");
    if (!f) return -1;

    fprintf(f,
            "P6\n"
            "%d %d\n"
            "255\n",
            image.width,
            image.height);

    for (int row = 0; row < image.height; ++row) {
        for (int col = 0; col < image.width; ++col) {
            Pixel32 p = *(image.pixels + image.width * row + col);
            fputc(p.r, f);
            fputc(p.g, f);
            fputc(p.b, f);
        }
    }

    fclose(f);

    return 0;
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

    for (int row = 0; (row < src->rows); ++row) {
        if (row + y < dest.height) {
            for (int col = 0; (col < src->width); ++col) {
                if (col + x < dest.width) {
                    float a = src->buffer[row * src->pitch + col] / 255.0f;
                    dest.pixels[(row + y) * dest.width + col + x].r =
                        a * color.r + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].r;
                    dest.pixels[(row + y) * dest.width + col + x].g =
                        a * color.g + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].g;
                    dest.pixels[(row + y) * dest.width + col + x].b =
                        a * color.b + (1.0f - a) * dest.pixels[(row + y) * dest.width + col + x].b;
                }
                // TODO: how do we mix alphas?
            }
        }
    }
}

void slap_onto_image32(Image32 dest, Image32 src, int x, int y)
{
    for (int row = 0; (row < src.height); ++row) {
        if (row + y < dest.height) {
            for (int col = 0; (col < src.width); ++col) {
                if (col + x < dest.width) {
                    dest.pixels[(row + y) * dest.width + col + x] =
                        src.pixels[row * src.height + col];
                }
            }
        }
    }
}

// TODO: slap_onto_image32 for libgif SavedImage does not support transparency
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

    for (int row = 0; (row < src->ImageDesc.Height); ++row) {
        if (row + y < dest.height) {
            for (int col = 0; (col < src->ImageDesc.Width); ++col) {
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

    for (int row = 0; (row < src->rows); ++row) {
        if(row + y < dest->rows) {
            for (int col = 0;
                 (col < src->width);
                 ++col) {
                if (col + x < dest->width) {
                    dest->buffer[(row + y) * dest->pitch + col + x] =
                        src->buffer[row * src->pitch + col];
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

    image.format = PNG_FORMAT_RGBA;
    Pixel32 *buffer = (Pixel32 *)malloc(sizeof(Pixel32) * image.width * image.height);
    png_image_finish_read(&image, NULL, buffer, 0, NULL);

    Image32 result = {
        .pixels = buffer,
        .width = image.width,
        .height = image.height
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
    for (int i = 0; i < text_count; ++i) {
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

constexpr size_t VODUS_FPS = 60;
constexpr float VODUS_DELTA_TIME = 1.0f / VODUS_FPS;
constexpr size_t VODUS_WIDTH = 1920;
constexpr size_t VODUS_HEIGHT = 1080;
constexpr size_t VODUS_QUEUE_CAPACITY = 1024;
constexpr float VODUS_VIDEO_DURATION = 5.0f;
constexpr size_t VODUS_OUTPUT_THREAD_COUNT = 4;

Image32 frame_arena[VODUS_QUEUE_CAPACITY];
std::atomic_size_t frame_arena_size = VODUS_QUEUE_CAPACITY;

void init_frame_arena(void)
{
    for (size_t i = 0; i < VODUS_QUEUE_CAPACITY; ++i) {
        frame_arena[i].width = VODUS_WIDTH;
        frame_arena[i].height = VODUS_HEIGHT;
        frame_arena[i].pixels =
            (Pixel32*) std::malloc(sizeof(Pixel32) * VODUS_WIDTH * VODUS_HEIGHT);
    }
}

Image32 alloc_frame(void)
{
    return frame_arena[--frame_arena_size];
}

void dealloc_frame(Image32 frame)
{
    frame_arena[frame_arena_size++] = frame;
}

Image32 queue[VODUS_QUEUE_CAPACITY];
size_t queue_begin = 0;
size_t queue_size = 0;

pthread_mutex_t queue_mutex;
pthread_t output_threads[VODUS_OUTPUT_THREAD_COUNT];
int frame_count = 0;

bool enqueue(Image32 frame)
{
    pthread_mutex_lock(&queue_mutex);
    defer(pthread_mutex_unlock(&queue_mutex));

    if (queue_size >= VODUS_QUEUE_CAPACITY) {
        return false;
    }

    queue[(queue_begin + queue_size) % VODUS_QUEUE_CAPACITY] = frame;
    queue_size += 1;
    return true;
}

Image32 dequeue(int *current_frame_count)
{
    pthread_mutex_lock(&queue_mutex);
    defer(pthread_mutex_unlock(&queue_mutex));

    if (queue_size == 0) {
        return {0, 0, nullptr};
    }

    Image32 result = queue[queue_begin % VODUS_QUEUE_CAPACITY];
    queue_size -= 1;
    queue_begin += 1;
    if (current_frame_count) *current_frame_count = frame_count++;

    return result;
}

std::atomic_bool stop_output_threads = false;

void *output_thread_routine(void*)
{
    constexpr size_t FILE_PATH_CAPACITY = 256;
    char file_path[FILE_PATH_CAPACITY];

    for (;;) {
        int frame_count;
        Image32 frame = dequeue(&frame_count);
        if (frame.pixels == nullptr) {
            if (stop_output_threads.load()) {
                return nullptr;
            }
            continue;
        }

        snprintf(file_path, FILE_PATH_CAPACITY, "output/%04d.png", frame_count++);
        save_image32_as_png(frame, file_path);
        dealloc_frame(frame);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: ./vodus <text> <gif_image> <png_image> [font]\n");
        exit(1);
    }

    const char *text = argv[1];
    const char *gif_filepath = argv[2];
    const char *png_filepath = argv[3];
    const char *face_file_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

    if (argc >= 5) {
        face_file_path = argv[4];
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

    float text_x = 0.0f;
    float text_y = VODUS_HEIGHT;

    // TODO: proper gif timings should be taken from the gif file itself
    constexpr float GIF_DURATION = 2.0f;

    float gif_dt = GIF_DURATION / gif_file->ImageCount;
    float t = 0.0f;

    Image32 png_sample_image = load_image32_from_png(png_filepath);

    pthread_mutex_init(&queue_mutex, nullptr);
    init_frame_arena();

    for (size_t i = 0; i < VODUS_OUTPUT_THREAD_COUNT; ++i) {
        pthread_create(output_threads + i, nullptr, output_thread_routine, nullptr);
    }

    while (text_y > 0.0f) {
        Image32 surface = alloc_frame();

        fill_image32_with_color(surface, {50, 0, 0, 255});

        size_t gif_index = (int)(t / gif_dt) % gif_file->ImageCount;

        assert(gif_file->ImageCount > 0);
        slap_onto_image32(surface,
                          &gif_file->SavedImages[gif_index],
                          gif_file->SColorMap,
                          (int) text_x, (int) text_y);
        slap_onto_image32(surface,
                          png_sample_image,
                          (int) text_x + gif_file->SavedImages[gif_index].ImageDesc.Width, (int) text_y);

        slap_text_onto_image32(surface, face, text, {0, 255, 0, 255},
                               (int) text_x, (int) text_y);

        while (!enqueue(surface)) {}

        text_y -= (VODUS_HEIGHT / VODUS_VIDEO_DURATION) * VODUS_DELTA_TIME;
        t += VODUS_DELTA_TIME;
    }
    stop_output_threads.store(true);
    printf("Finished rendering. Waiting for the output thread.\n");

    for (size_t i = 0; i < VODUS_OUTPUT_THREAD_COUNT; ++i) {
        pthread_join(output_threads[i], nullptr);
    }

    return 0;
}
