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

void avec(int code)
{
    if (code < 0) {
        constexpr size_t buf_size = 256;
        char buf[buf_size];
        av_strerror(code, buf, buf_size);
        fprintf(stderr, "ffmpeg pooped itself: %s\n", buf);
        exit(1);
    }
}

template <typename T>
T *fail_if_null(T *ptr, const char *format, ...)
{
    if (ptr == nullptr) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        exit(1);
    }

    return ptr;
}

constexpr size_t VODUS_FPS = 60;
constexpr float VODUS_DELTA_TIME = 1.0f / VODUS_FPS;
constexpr size_t VODUS_WIDTH = 1920;
constexpr size_t VODUS_HEIGHT = 1080;
constexpr float VODUS_VIDEO_DURATION = 5.0f;

void encode_avframe(AVCodecContext *context, AVFrame *frame, AVPacket *pkt, FILE *outfile)
{
    if (frame) {
        printf("Send frame %3" PRId64 "\n", frame->pts);
    }

    avec(avcodec_send_frame(context, frame));

    for (int ret = 0; ret >= 0; ) {
        ret = avcodec_receive_packet(context, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else {
            avec(ret);
        }

        printf("Write packet %3" PRId64 " (size=%5d)\n", pkt->pts, pkt->size);
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

void slap_image32_onto_avframe(Image32 frame_image32, AVFrame *avframe)
{
    assert(avframe->width == (int) frame_image32.width);
    assert(avframe->height == (int) frame_image32.height);

    for (int y = 0; y < avframe->height; ++y) {
        for (int x = 0; x < avframe->width; ++x) {
            Pixel32 p = frame_image32.pixels[y * frame_image32.width + x];
            int Y =  (0.257 * p.r) + (0.504 * p.g) + (0.098 * p.b) + 16;
            int V =  (0.439 * p.r) - (0.368 * p.g) - (0.071 * p.b) + 128;
            int U = -(0.148 * p.r) - (0.291 * p.g) + (0.439 * p.b) + 128;
            avframe->data[0][y        * avframe->linesize[0] + x]        = Y;
            avframe->data[1][(y >> 1) * avframe->linesize[1] + (x >> 1)] = U;
            avframe->data[2][(y >> 1) * avframe->linesize[2] + (x >> 1)] = V;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 6) {
        fprintf(stderr, "Usage: ./vodus <text> <gif_image> <png_image> <font> <output>\n");
        exit(1);
    }

    const char *text = argv[1];
    const char *gif_filepath = argv[2];
    const char *png_filepath = argv[3];
    const char *face_file_path = argv[4];
    const char *output_filepath = argv[5];

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
        fprintf(stderr, "Could not read gif file: %s\n", gif_filepath);
        exit(1);
    }
    assert(error == 0);
    DGifSlurp(gif_file);

    float text_x = 0.0f;
    float text_y = VODUS_HEIGHT;

    // TODO(#7): proper gif timings should be taken from the gif file itself
    constexpr float GIF_DURATION = 2.0f;

    float gif_dt = GIF_DURATION / gif_file->ImageCount;
    float t = 0.0f;

    Image32 png_sample_image = load_image32_from_png(png_filepath);
    Image32 surface = {
        .width = VODUS_WIDTH,
        .height = VODUS_HEIGHT,
        .pixels = new Pixel32[VODUS_WIDTH * VODUS_HEIGHT]
    };

    // FFMPEG INIT START //////////////////////////////
    AVCodec *codec = fail_if_null(
        avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO),
        "Codec not found");

    AVCodecContext *context = fail_if_null(
        avcodec_alloc_context3(codec),
        "Could not allocate video codec context\n");
    defer(avcodec_free_context(&context));

    context->bit_rate = 400'000;
    static_assert(VODUS_WIDTH % 2 == 0, "Resolution must be multiple of two");
    static_assert(VODUS_HEIGHT % 2 == 0, "Resolution must be multiple of two");
    context->width = VODUS_WIDTH;
    context->height = VODUS_HEIGHT;
    context->time_base = (AVRational){1, VODUS_FPS};
    context->framerate = (AVRational){VODUS_FPS, 1};
    context->gop_size = 10;
    context->max_b_frames = 1;
    context->pix_fmt = AV_PIX_FMT_YUV420P;

    AVPacket *pkt = fail_if_null(
        av_packet_alloc(),
        "Could not allocate packet\n");
    defer(av_packet_free(&pkt));

    avec(avcodec_open2(context, codec, NULL));

    FILE *f = fail_if_null(
        fopen(output_filepath, "wb"),
        "Could not open %s\n", output_filepath);
    defer(fclose(f));

    AVFrame *frame = fail_if_null(
        av_frame_alloc(),
        "Could not allocate video frame\n");
    defer(av_frame_free(&frame));

    frame->format = context->pix_fmt;
    frame->width  = context->width;
    frame->height = context->height;

    avec(av_frame_get_buffer(frame, 32));
    // FFMPEG INIT STOP //////////////////////////////

    for (int frame_index = 0; text_y > 0.0f; ++frame_index) {
        fill_image32_with_color(surface, {50, 0, 0, 255});

        size_t gif_index = (int)(t / gif_dt) % gif_file->ImageCount;

        GraphicsControlBlock gcb;
        int ok = DGifSavedExtensionToGCB(gif_file, gif_index, &gcb);
        assert(ok);

        assert(gif_file->ImageCount > 0);
        slap_savedimage_onto_image32(
            surface,
            &gif_file->SavedImages[gif_index],
            gif_file->SColorMap,
            gcb,
            (int) text_x, (int) text_y);
        slap_image32_onto_image32(
            surface,
            png_sample_image,
            (int) text_x + gif_file->SavedImages[gif_index].ImageDesc.Width, (int) text_y);

        slap_text_onto_image32(surface, face, text, {0, 255, 0, 255},
                               (int) text_x, (int) text_y);

        slap_image32_onto_avframe(surface, frame);

        frame->pts = frame_index;
        encode_avframe(context, frame, pkt, f);

        text_y -= (VODUS_HEIGHT / VODUS_VIDEO_DURATION) * VODUS_DELTA_TIME;
        t += VODUS_DELTA_TIME;
    }

    encode_avframe(context, NULL, pkt, f);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), f);

    return 0;
}
