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
        const size_t buf_size = 256;
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

struct Message
{
    time_t timestamp;
    const char *nickname;
    const char *message;
};

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

Message messages[] = {
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
    {0, "Nice_la", "hello         AYAYA /"},
    {1, "Zuglya", "\\o/"},
    {1, "Tsoding", "phpHop"},
    {2, "Tsoding", "phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop phpHop"},
    {2, "recursivechat", "me me me"},
    {3, "nuffleee", "because dumb AYAYA compiler"},
    {4, "marko8137", "hi phpHop"},
    {5, "nulligor", "meme?"},
    {6, "mbgoodman", "KKool"},
    {7, "Tsoding", "Jebaited"},
};

using Encode_Frame = std::function<void(Image32, int)>;

void render_message(Image32 surface, FT_Face face,
                    Message message,
                    int *x, int *y,
                    Bttv *bttv)
{
    assert(bttv);

    slap_text_onto_image32_wrapped(surface,
                                   face,
                                   message.nickname,
                                   {255, 0, 0, 255},
                                   x, y);

    slap_text_onto_image32_wrapped(surface,
                                   face,
                                   ": ",
                                   {255, 0, 0, 255},
                                   x, y);

    auto text = cstr_as_string_view(message.message).trim();
    while (text.count > 0) {
        auto word = text.chop_word();
        auto maybe_bttv_emote = bttv->emote_by_name(word);

        // TODO(#21): FFZ emotes are not rendered
        // TODO(#23): Twitch emotes are not rendered

        if (maybe_bttv_emote.has_value) {
            auto bttv_emote = maybe_bttv_emote.unwrap;

            const float emote_ratio = (float) bttv_emote.width() / bttv_emote.height();
            const int emote_height = VODUS_FONT_SIZE;
            const int emote_width = floorf(emote_height * emote_ratio);

            if (*x + emote_width >= (int)surface.width) {
                *x = 0;
                // TODO(#31): the size of the font in word wrapping should be taken from the face itself
                //   Right now font size is hardcoded.
                //   Grep for `@word-wrap-face` to find all of the hardcoded places
                *y += VODUS_FONT_SIZE;
            }

            bttv_emote.slap_onto_image32(surface,
                                         *x, *y - emote_height,
                                         emote_width, emote_height);
            *x += emote_width;
        } else {
            slap_text_onto_image32_wrapped(surface,
                                           face,
                                           word,
                                           {0, 255, 0, 255},
                                           x, y);
        }

        slap_text_onto_image32_wrapped(surface,
                                       face,
                                       " ",
                                       {0, 255, 0, 255},
                                       x, y);
    }
}

bool render_log(Image32 surface, FT_Face face,
                size_t message_begin,
                size_t message_end,
                Bttv *bttv)
{
    fill_image32_with_color(surface, {0, 0, 0, 255});
    const int FONT_HEIGHT = VODUS_FONT_SIZE;
    const int CHAT_PADDING = 0;
    int text_y = FONT_HEIGHT + CHAT_PADDING;
    for (size_t i = message_begin; i < message_end; ++i) {
        int text_x = 0;
        render_message(surface, face, messages[i], &text_x, &text_y, bttv);
        text_y += FONT_HEIGHT + CHAT_PADDING;
    }
    return text_y > (int)surface.height;
}

void sample_chat_log_animation(FT_Face face, Encode_Frame encode_frame, Bttv *bttv)
{
    assert(bttv);

    Image32 surface = {
        .width = VODUS_WIDTH,
        .height = VODUS_HEIGHT,
        .pixels = new Pixel32[VODUS_WIDTH * VODUS_HEIGHT]
    };
    defer(delete[] surface.pixels);

    size_t message_begin = 0;
    size_t message_end = 0;
    float message_cooldown = 0.0f;
    size_t frame_index = 0;
    for (; message_end < ARRAY_SIZE(messages); ++frame_index) {
        printf("Current message_end = %ld\n", message_end);

        if (message_cooldown <= 0.0f) {
            message_end += 1;
            auto t1 = messages[message_end - 1].timestamp;
            auto t2 = messages[message_end].timestamp;
            message_cooldown = t2 - t1;
        }

        message_cooldown -= VODUS_DELTA_TIME_SEC;

        // TODO(#16): animate appearance of the message
        // TODO(#33): scroll implementation simply rerenders frames until they fit the screen which might be slow
        while (render_log(surface, face, message_begin, message_end, bttv) &&
               message_begin < ARRAY_SIZE(messages)) {
            message_begin++;
        }
        encode_frame(surface, frame_index);

        bttv->update(VODUS_DELTA_TIME_SEC);
    }

    const size_t TRAILING_BUFFER_SEC = 2;
    for (size_t i = 0; i < TRAILING_BUFFER_SEC * VODUS_FPS; ++i, ++frame_index) {
        while (render_log(surface, face, message_begin, message_end, bttv) &&
               message_begin < ARRAY_SIZE(messages)) {
            message_begin++;
        }
        bttv->update(VODUS_DELTA_TIME_SEC);
        encode_frame(surface, frame_index);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Usage: ./vodus <gif_image> <png_image> <font> <output>\n");
        exit(1);
    }

    const char *gif_filepath = argv[1];
    const char *png_filepath = argv[2];
    const char *face_file_path = argv[3];
    const char *output_filepath = argv[4];

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

    error = FT_Set_Pixel_Sizes(face, 0, VODUS_FONT_SIZE);
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

    auto png_sample_image = load_image32_from_png(png_filepath);

    Bttv bttv = { png_sample_image, Gif_Animat {gif_file} };

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

    AVPacket *packet = fail_if_null(
        av_packet_alloc(),
        "Could not allocate packet\n");
    defer(av_packet_free(&packet));

    avec(avcodec_open2(context, codec, NULL));

    FILE *output_stream = fail_if_null(
        fopen(output_filepath, "wb"),
        "Could not open %s\n", output_filepath);
    defer(fclose(output_stream));

    AVFrame *frame = fail_if_null(
        av_frame_alloc(),
        "Could not allocate video frame\n");
    defer(av_frame_free(&frame));

    frame->format = context->pix_fmt;
    frame->width  = context->width;
    frame->height = context->height;

    avec(av_frame_get_buffer(frame, 32));
    // FFMPEG INIT STOP //////////////////////////////

    auto encode_frame =
        [frame, context, packet, output_stream](Image32 surface, int frame_index) {
            slap_image32_onto_avframe(surface, frame);
            frame->pts = frame_index;
            encode_avframe(context, frame, packet, output_stream);
        };

    for (size_t i = 0; i < ARRAY_SIZE(messages); ++i) {
        messages[i].timestamp = i;
    }

    sample_chat_log_animation(face, encode_frame, &bttv);

    encode_avframe(context, NULL, packet, output_stream);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), output_stream);

    return 0;
}
