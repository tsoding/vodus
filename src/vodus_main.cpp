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
    String_View nickname;
    String_View message;
};

Message messages[VODUS_MESSAGES_CAPACITY];
size_t messages_size = 0;

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

    auto text = message.message.trim();
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
    for (; message_end < messages_size; ++frame_index) {
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
               message_begin < messages_size) {
            message_begin++;
        }
        encode_frame(surface, frame_index);

        bttv->update(VODUS_DELTA_TIME_SEC);
    }

    const size_t TRAILING_BUFFER_SEC = 2;
    for (size_t i = 0; i < TRAILING_BUFFER_SEC * VODUS_FPS; ++i, ++frame_index) {
        while (render_log(surface, face, message_begin, message_end, bttv) &&
               message_begin < messages_size) {
            message_begin++;
        }
        bttv->update(VODUS_DELTA_TIME_SEC);
        encode_frame(surface, frame_index);
    }
}

void expect_char(String_View *input, char x)
{
    if (input->count == 0 || *input->data != x) {
        println(stderr, "Expected '", x, "'");
        abort();
    }
    input->chop(1);
}

String_View chop_digits(String_View *input)
{
    String_View digits = { 0, input->data };
    while (input->count > 0 && isdigit(*input->data)) {
        digits.count++;
        input->chop(1);
    }
    return digits;
}

time_t chop_timestamp(String_View *input)
{
    *input = input->trim();

    expect_char(input, '[');
    String_View hours = chop_digits(input);
    expect_char(input, ':');
    String_View minutes = chop_digits(input);
    expect_char(input, ':');
    String_View seconds = chop_digits(input);
    expect_char(input, ']');

    const time_t timestamp =
        hours.as_integer<time_t>().unwrap * 60 * 60 +
        minutes.as_integer<time_t>().unwrap * 60 +
        seconds.as_integer<time_t>().unwrap;

    return timestamp;
}

String_View chop_nickname(String_View *input)
{
    *input = input->trim();
    expect_char(input, '<');
    return input->chop_by_delim('>');
}

void usage(FILE *stream)
{
    println(stream, "Usage: vodus [OPTIONS] <log-filepath>");
    println(stream, "    --help|-h                 Display this help and exit");
    println(stream, "    --output|-o <filepath>    Output path");
    println(stream, "    --sample-gif <filepath>   Path to a sample GIF image");
    println(stream, "    --sample-png <filepath>   Path to a sample PNG image");
    println(stream, "    --font <filepath>         Path to the Font face file");
    println(stream, "    --limit <number>          Limit the amout of messages to render");
}

int main(int argc, char *argv[])
{
    const char *log_filepath = nullptr;
    const char *gif_filepath = nullptr;
    const char *png_filepath = nullptr;
    const char *face_filepath = nullptr;
    const char *output_filepath = nullptr;
    size_t messages_limit = VODUS_MESSAGES_CAPACITY;

    for (int i = 1; i < argc;) {
        const char *arg = argv[i];

#define BEGIN_PARAMETER(name)                                           \
        if (i + 1 >= argc) {                                            \
            println(stderr, "Error: No argument is provided for `", arg, "`"); \
            usage(stderr);                                              \
            exit(1);                                                    \
        }                                                               \
        const char * name = argv[i + 1]

#define END_PARAMETER i += 2

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)  {
            usage(stdout);
            exit(0);
        } else if (strcmp(arg, "--sample-gif") == 0) {
            BEGIN_PARAMETER(filepath);
            gif_filepath = filepath;
            END_PARAMETER;
        } else if (strcmp(arg, "--sample-png") == 0) {
            BEGIN_PARAMETER(filepath);
            png_filepath = filepath;
            END_PARAMETER;
        } else if (strcmp(arg, "--font") == 0) {
            BEGIN_PARAMETER(filepath);
            face_filepath = filepath;
            END_PARAMETER;
        } else if (strcmp(arg, "--output") == 0 || strcmp(arg, "-o") == 0) {
            BEGIN_PARAMETER(filepath);
            output_filepath = filepath;
            END_PARAMETER;
        } else if (strcmp(arg, "--limit") == 0) {
            BEGIN_PARAMETER(limit_cstr);

            auto limit_maybe_integer = cstr_as_string_view(limit_cstr).as_integer<size_t>();
            if (!limit_maybe_integer.has_value) {
                println(stderr, "Error: `", arg, "` is not an integer");
                usage(stderr);
                exit(1);
            }
            messages_limit = limit_maybe_integer.unwrap;

            END_PARAMETER;
        } else {
            if (log_filepath != nullptr) {
                println(stderr, "Error: Input log file is provided twice");
                usage(stderr);
                exit(1);
            }

            log_filepath = argv[i];

            i += 1;
        }

#undef END_PARAMETER
#undef BEGIN_PARAMETER
    }

    if (face_filepath == nullptr) {
        println(stderr, "Error: Font was not provided. Please use `--font` flag.");
        usage(stderr);
        exit(1);
    }


    FT_Library library;
    auto error = FT_Init_FreeType(&library);
    if (error) {
        fprintf(stderr, "Could not initialize FreeType2\n");
        exit(1);
    }

    FT_Face face;
    error = FT_New_Face(library,
                        face_filepath,
                        0,
                        &face);
    if (error == FT_Err_Unknown_File_Format) {
        fprintf(stderr,
                "%s appears to have an unsuppoted format\n",
                face_filepath);
        exit(1);
    } else if (error) {
        fprintf(stderr,
                "%s could not be opened\n",
                face_filepath);
        exit(1);
    }

    printf("Loaded %s\n", face_filepath);
    printf("\tnum_glyphs = %ld\n", face->num_glyphs);

    error = FT_Set_Pixel_Sizes(face, 0, VODUS_FONT_SIZE);
    if (error) {
        fprintf(stderr, "Could not set font size in pixels\n");
        exit(1);
    }

    Gif_Animat sample_gif_animat = {};
    if (gif_filepath) {
        sample_gif_animat.file = DGifOpenFileName(gif_filepath, &error);
        if (error) {
            fprintf(stderr, "Could not read gif file: %s\n", gif_filepath);
            exit(1);
        }
        assert(error == 0);
        DGifSlurp(sample_gif_animat.file);
    }

    Image32 sample_png_image = {};
    if (png_filepath) {
        sample_png_image = load_image32_from_png(png_filepath);
    }

    Bttv bttv = { };
    auto mapping_csv = file_as_string_view("./mapping.csv");
    while (mapping_csv.count > 0) {
        auto line = mapping_csv.chop_by_delim('\n');
        auto name = line.chop_by_delim(',');
        bttv.add_mapping(name, line);
    }

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

    if (output_filepath == nullptr) {
        println(stderr, "Error: Output filepath is not provided. Use `--output` flag.");
        usage(stderr);
        exit(1);
    }

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

    // TODO(#35): log is not retrived directly from the Twitch API
    //   See https://github.com/PetterKraabol/Twitch-Chat-Downloader
    if (log_filepath == nullptr) {
        println(stderr, "Input log file is not provided");
        usage(stderr);
        exit(1);
    }
    String_View input = file_as_string_view(log_filepath);
    while (input.count > 0) {
        assert(messages_size < VODUS_MESSAGES_CAPACITY);
        String_View message = input.chop_by_delim('\n');
        messages[messages_size].timestamp = (int) chop_timestamp(&message);
        messages[messages_size].nickname = chop_nickname(&message);
        messages[messages_size].message = message.trim();
        messages_size++;
    }
    messages_size = std::min(messages_size, messages_limit);
    std::sort(messages, messages + messages_size,
              [](const Message &m1, const Message &m2) {
                  return m1.timestamp < m2.timestamp;
              });

    sample_chat_log_animation(face, encode_frame, &bttv);

    encode_avframe(context, NULL, packet, output_stream);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), output_stream);

    return 0;
}
