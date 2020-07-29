void avec(int code)
{
    if (code < 0) {
        const size_t buf_size = 256;
        char buf[buf_size];
        av_strerror(code, buf, buf_size);
        println(stderr, "ffmpeg pooped itself: ", buf);
        exit(1);
    }
}

template <typename T, typename... Args>
T *fail_if_null(T *ptr, Args... args)
{
    if (ptr == nullptr) {
        println(stderr, args...);
        exit(1);
    }

    return ptr;
}

void encode_avframe(AVCodecContext *context, AVFrame *frame, AVPacket *pkt, FILE *outfile)
{
    avec(avcodec_send_frame(context, frame));

    for (int ret = 0; ret >= 0; ) {
        ret = avcodec_receive_packet(context, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else {
            avec(ret);
        }

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

void render_message(Image32 surface, FT_Face face,
                    Message message,
                    int *x, int *y,
                    Emote_Cache *emote_cache,
                    Video_Params params)
{
    assert(emote_cache);

    slap_text_onto_image32_wrapped(surface,
                                   face,
                                   message.nickname,
                                   params.nickname_color,
                                   x, y,
                                   params.font_size);


    auto text = message.message.trim();
    auto text_color = params.text_color;
    const char *nick_text_sep = ": ";

    const auto slash_me = "/me"_sv;

    if (text.has_prefix(slash_me)) {
        text.chop(slash_me.count);
        text_color = params.nickname_color;
        nick_text_sep = " ";
    }

    slap_text_onto_image32_wrapped(surface,
                                   face,
                                   nick_text_sep,
                                   params.nickname_color,
                                   x, y,
                                   params.font_size);

    while (text.count > 0) {
        auto word = text.chop_word();
        auto maybe_emote = emote_cache->emote_by_name(word);

        // TODO(#23): Twitch emotes are not rendered
        if (maybe_emote.has_value) {
            auto emote = maybe_emote.unwrap;

            if (*x + emote.width() >= (int)surface.width) {
                *x = 0;
                *y += params.font_size;
            }

            emote.slap_onto_image32(surface, *x, *y - emote.height());
            *x += emote.width();
        } else {
            slap_text_onto_image32_wrapped(surface,
                                           face,
                                           word,
                                           text_color,
                                           x, y,
                                           params.font_size);
        }

        slap_text_onto_image32_wrapped(surface,
                                       face,
                                       " ",
                                       text_color,
                                       x, y,
                                       params.font_size);
    }
}

struct Message_Entry
{
    Message message;
    float a;

    float render(Image32 surface,
                 FT_Face face,
                 Emote_Cache *emote_cache,
                 Video_Params params,
                 float y)
    {
        int pen_x = 0;
        int pen_y = (int) floorf(y + params.font_size);
        render_message(surface, face, message, &pen_x, &pen_y, emote_cache, params);
        return pen_y - y;
    }

    float height(FT_Face face,
                 Emote_Cache *emote_cache,
                 Video_Params params)
    {
        Image32 fake_surface = {};
        fake_surface.width = params.width;
        return render(fake_surface, face, emote_cache, params, 0.0f);
    }
};

struct Frame_Encoder
{
    AVFrame *frame;
    AVCodecContext *context;
    AVPacket *packet;
    FILE *output_stream;

    void encode_frame(Image32 surface, int frame_index)
    {
        slap_image32_onto_avframe(surface, frame);
        frame->pts = frame_index;
        encode_avframe(context, frame, packet, output_stream);
    }
};

template <typename T, size_t Capacity>
struct Queue
{
    T items[Capacity];
    size_t begin;
    size_t count;

    T &operator[](size_t index)
    {
        return items[(begin + index) % Capacity];
    }

    const T &operator[](size_t index) const
    {
        return items[(begin + index) % Capacity];
    }

    T &first()
    {
        assert(count > 0);
        return items[begin];
    }

    void enqueue(T x)
    {
        assert(count < Capacity);
        items[(begin + count) % Capacity] = x;
        count++;
    }

    T dequeue()
    {
        assert(count > 0);
        T result = items[begin];
        begin = (begin + 1) % Capacity;
        count--;
        return result;
    }
};

template <size_t Capacity>
struct Message_Entry_Buffer
{
    Queue<Message_Entry, Capacity> entering;
    Queue<Message_Entry, Capacity> active;
    Queue<Message_Entry, Capacity> leaving;

    // TODO(#74): message entering/leaving animation should be disablable
    // TODO(#75): entering/leaving animation should also animate alpha

    void push(Message message)
    {
        Message_Entry entry = {};
        entry.message = message;
        entry.a = 0.0f;
        entering.enqueue(entry);
    }

    void pop()
    {
        if (active.count > 0) {
            auto entry = active.dequeue();
            entry.a = 0.0f;
            leaving.enqueue(entry);
        }
    }

    void update(float dt)
    {
        // TODO(#76): easing in/out for message entering/leaving animations
        // TODO(#77): parameters of message entering/leaving animations should be customizable
        const float ALPHA_VELOCITY = 1.0f / 0.1f;

        for (size_t i = 0; i < entering.count; ++i) {
            entering[i].a += ALPHA_VELOCITY * dt;
        }

        while (entering.count > 0 && entering.first().a >= 1.0f) {
            auto entry = entering.dequeue();
            active.enqueue(entry);
        }

        for (size_t i = 0; i < leaving.count; ++i) {
            leaving[i].a += ALPHA_VELOCITY * dt;
        }

        while (leaving.count > 0 && leaving.first().a >= 1.0f) {
            leaving.dequeue();
        }
    }

    float render(Image32 surface,
                 FT_Face face,
                 Emote_Cache *emote_cache,
                 Video_Params params)
    {
        float y = 0.0f;

        for (size_t i = 0; i < leaving.count; ++i) {
            auto &entry = leaving[i];
            const float h = entry.height(face, emote_cache, params);
            y -= h * entry.a;
            entry.render(surface, face, emote_cache, params, y);
            y += h;
        }

        for (size_t i = 0; i < active.count; ++i) {
            auto &entry = active[i];
            const float h = entry.render(surface, face, emote_cache, params, y);
            y += h;
        }

        for (size_t i = 0; i < entering.count; ++i) {
            auto &entry = entering[i];
            const float h = entry.height(face, emote_cache, params);
            y += h * (1.0f - entry.a);
            entry.render(surface, face, emote_cache, params, y);
            y += h;
        }

        return y;
    }
};

Message_Entry_Buffer<1024> message_entry_buffer = {};

void sample_chat_log_animation(FT_Face face,
                               Frame_Encoder *encoder,
                               Emote_Cache *emote_cache,
                               Video_Params params)
{
    assert(emote_cache);

    Image32 surface = {
        .width = params.width,
        .height = params.height,
        .pixels = new Pixel32[params.width * params.height]
    };
    defer(delete[] surface.pixels);

    size_t message_end = 0;
    float message_cooldown = 0.0f;
    size_t frame_index = 0;
    float t = 0.0f;

    const float VODUS_DELTA_TIME_SEC = 1.0f / params.fps;
    const size_t TRAILING_BUFFER_SEC = 2;
    assert(messages_size > 0);
    const float total_t = messages[messages_size - 1].timestamp + TRAILING_BUFFER_SEC;
    float h = 0.0f;
    for (; message_end < messages_size; ++frame_index) {
        if (message_cooldown <= 0.0f) {
            message_entry_buffer.push(messages[message_end]);
            if (h >= params.height) {
                // TODO(#78): messages are rendered sometimes outside of the windows
                //   Just render sample.txt to reproduce.
                message_entry_buffer.pop();
            }

            message_end += 1;
            auto t1 = messages[message_end - 1].timestamp;
            auto t2 = messages[message_end].timestamp;
            message_cooldown = t2 - t1;
        }

        message_cooldown -= VODUS_DELTA_TIME_SEC;

        fill_image32_with_color(surface, params.background_color);
        h = message_entry_buffer.render(surface, face, emote_cache, params);
        encoder->encode_frame(surface, frame_index);

        t += VODUS_DELTA_TIME_SEC;
        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_entry_buffer.update(VODUS_DELTA_TIME_SEC);

        print(stdout, "\rRendered ", (int) roundf(t), "/", (int) roundf(total_t), " seconds");
    }

    for (size_t i = 0; i < TRAILING_BUFFER_SEC * params.fps; ++i, ++frame_index) {
        fill_image32_with_color(surface, params.background_color);
        message_entry_buffer.render(surface, face, emote_cache, params);

        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_entry_buffer.update(VODUS_DELTA_TIME_SEC);
        encoder->encode_frame(surface, frame_index);

        t += VODUS_DELTA_TIME_SEC;
        print(stdout, "\rRendered ", (int) roundf(t), "/", (int) roundf(total_t), " seconds");
    }

    print(stdout, "\rRendered ", (int) roundf(total_t), "/", (int) roundf(total_t), " seconds");
    print(stdout, "\n");
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
    // TODO(#79): usage output is outdated
    println(stream, "Usage: vodus [OPTIONS] <log-filepath>");
    println(stream, "    --help|-h                       Display this help and exit");
    println(stream, "    --output|-o <filepath>          Output path");
    println(stream, "    --font <filepath>               Path to the Font face file");
    println(stream, "    --font-size <number>            The size of the Font face");
    println(stream, "    --limit <number>                Limit the amount of messages to render");
    println(stream, "    --width <number>                Width of the output video");
    println(stream, "    --height <number>               Height of the output video");
    println(stream, "    --fps <number>                  Frames per second of the output video");
    println(stream, "    --background-color <color-hex>  Color of the background");
    println(stream, "    --nickname-color <color-hex>    Color of the nickname part of the rendered message");
    println(stream, "    --text-color <color-hex>        Color of the text part of the rendered message");
    println(stream, "    --bitrate <number>              The average bitrate in bits/s (probably, libavcodec\n"
                    "                                    didn't really tell me the exact units)");
    println(stream, "  Values:");
    println(stream, "    <filepath>                      Path to a file according to the platform format");
    println(stream, "    <number>                        Positive integer");
    println(stream, "    <color-hex>                     8 hex characters digits describing RGBA.\n"
                    "                                    Case insensitive. No leading # character.\n"
                    "                                    Examples: ffffffff, 000000FF, A0eA0000");
}

Maybe<Pixel32> hexstr_as_pixel32(String_View hexstr)
{
    if (hexstr.count != 8) return {};

    Pixel32 result = {};
    unwrap_into(result.r, hexstr.subview(0, 2).from_hex<uint8_t>());
    unwrap_into(result.g, hexstr.subview(2, 2).from_hex<uint8_t>());
    unwrap_into(result.b, hexstr.subview(4, 2).from_hex<uint8_t>());
    unwrap_into(result.a, hexstr.subview(6, 2).from_hex<uint8_t>());
    return {true, result};
}

int main_()
{
    void *iterator = NULL;
    const AVCodec *codec = NULL;
    println(stdout, "Probably available codecs:");
    while ((codec = av_codec_iterate(&iterator))) {
        if (avcodec_find_encoder_by_name(codec->name)) {
            println(stdout, "  ", codec->name);
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *log_filepath = nullptr;
    const char *face_filepath = nullptr;
    const char *output_filepath = nullptr;
    size_t messages_limit = VODUS_MESSAGES_CAPACITY;

    Video_Params params = {};
    params.fps               = 60;
    params.width             = 1920;
    params.height            = 1080;
    params.font_size         = 128;
    params.background_color = {32, 32, 32, 255};
    params.nickname_color   = {255, 100, 100, 255};
    params.text_color       = {200, 200, 200, 255};
    params.bitrate           = 400'000;

    for (int i = 1; i < argc;) {
        const auto arg = cstr_as_string_view(argv[i]);

#define BEGIN_PARAMETER(name)                                           \
        if (i + 1 >= argc) {                                            \
            println(stderr, "Error: No argument is provided for `", arg, "`"); \
            usage(stderr);                                              \
            exit(1);                                                    \
        }                                                               \
        const auto name = argv[i + 1];

#define END_PARAMETER i += 2

#define INTEGER_PARAMETER(type, variable)                                 \
        do {                                                              \
            BEGIN_PARAMETER(cstr);                                        \
            auto maybe = cstr_as_string_view(cstr).as_integer<type>();    \
            if (!maybe.has_value) {                                       \
                println(stderr, "Error: `", cstr, "` is not an integer"); \
                usage(stderr);                                            \
                exit(1);                                                  \
            }                                                             \
            variable = maybe.unwrap;                                      \
            END_PARAMETER;                                                \
        } while (0)

#define CSTR_PARAMETER(variable)                \
        do {                                    \
            BEGIN_PARAMETER(cstr);              \
            variable = cstr;                    \
            END_PARAMETER;                      \
        } while (0)

#define COLOR_PARAMETER(place)                                          \
        do {                                                            \
            BEGIN_PARAMETER(cstr);                                      \
            auto maybe = hexstr_as_pixel32(cstr_as_string_view(cstr));  \
            if (!maybe.has_value) {                                     \
                println(stderr, "Error: `", cstr, "` is not a hexstr of a color"); \
                usage(stderr);                                          \
                exit(1);                                                \
            }                                                           \
            place = maybe.unwrap;                                       \
            END_PARAMETER;                                              \
        } while (0)

        if (arg == "--help"_sv || arg == "-h"_sv)  {
            usage(stdout);
            exit(0);
        } else if (arg == "--font"_sv) {
            CSTR_PARAMETER(face_filepath);
        } else if (arg == "--output"_sv || arg == "-o"_sv) {
            CSTR_PARAMETER(output_filepath);
        } else if (arg == "--limit"_sv) {
            INTEGER_PARAMETER(size_t, messages_limit);
        } else if (arg == "--width"_sv) {
            INTEGER_PARAMETER(size_t, params.width);
        } else if (arg == "--height"_sv) {
            INTEGER_PARAMETER(size_t, params.height);
        } else if (arg == "--fps"_sv) {
            INTEGER_PARAMETER(size_t, params.fps);
        } else if (arg == "--font-size"_sv) {
            INTEGER_PARAMETER(size_t, params.font_size);
        } else if (arg == "--background-color"_sv) {
            COLOR_PARAMETER(params.background_color);
        } else if (arg == "--nickname-color"_sv) {
            COLOR_PARAMETER(params.nickname_color);
        } else if (arg == "--text-color"_sv) {
            COLOR_PARAMETER(params.text_color);
        } else if (arg == "--bitrate"_sv) {
            INTEGER_PARAMETER(int, params.bitrate);
        } else if (arg.has_prefix("-"_sv)) {
            println(stderr, "Error: Unknown flag `", arg, "`");
            usage(stderr);
            exit(1);
        } else {
            // TODO(#68): multiple input log files support?
            if (log_filepath != nullptr) {
                println(stderr, "Error: Input log file is provided twice");
                usage(stderr);
                exit(1);
            }

            log_filepath = argv[i];

            i += 1;
        }

#undef CSTR_PARAMETER
#undef INTEGER_PARAMETER
#undef END_PARAMETER
#undef BEGIN_PARAMETER
    }

    println(stdout, "params = ", params);

    if (params.width % 2 != 0 || params.height % 2 != 0) {
        println(stderr, "Error: resolution must be multiple of two!");
        exit(1);
    }

    if (face_filepath == nullptr) {
        println(stderr, "Error: Font was not provided. Please use `--font` flag.");
        usage(stderr);
        exit(1);
    }

    FT_Library library;
    auto error = FT_Init_FreeType(&library);
    if (error) {
        println(stderr, "Could not initialize FreeType2");
        exit(1);
    }

    FT_Face face;
    error = FT_New_Face(library,
                        face_filepath,
                        0,
                        &face);
    if (error == FT_Err_Unknown_File_Format) {
        println(stderr, "`", face_filepath, "` appears to have an unsuppoted format");
        exit(1);
    } else if (error) {
        println(stderr, "`", face_filepath, "` could not be opened\n");
        exit(1);
    }

    println(stdout, "Loaded ", face_filepath);
    println(stdout, "\tnum_glyphs = ", face->num_glyphs);

    error = FT_Set_Pixel_Sizes(face, 0, params.font_size);
    if (error) {
        println(stderr, "Could not set font size in pixels");
        exit(1);
    }

    // TODO(#44): BTTV mapping is not auto populated from the BTTV API
    // TODO(#45): FFZ mapping is not auto populated from the FFZ API
    // TODO: loading emotes step is too slow
    // Can we speed it up?
    Emote_Cache emote_cache = { };
    {
        clock_t begin = clock();
        emote_cache.populate_from_file("./mapping.csv", params.font_size);
        println(stdout, "Loading emotes took ", (float) (clock() - begin) / (float) CLOCKS_PER_SEC, " seconds");
    }

    // FFMPEG INIT START //////////////////////////////
    AVCodec *codec = fail_if_null(
        avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO),
        "Codec not found");

    AVCodecContext *context = fail_if_null(
        avcodec_alloc_context3(codec),
        "Could not allocate video codec context");
    defer(avcodec_free_context(&context));

    context->bit_rate = params.bitrate;
    context->width = params.width;
    context->height = params.height;
    context->time_base = (AVRational){1, (int) params.fps};
    context->framerate = (AVRational){(int) params.fps, 1};
    context->gop_size = 10;
    // context->max_b_frames = 1;
    context->pix_fmt = AV_PIX_FMT_YUV420P;

    AVPacket *packet = fail_if_null(
        av_packet_alloc(),
        "Could not allocate packet");
    defer(av_packet_free(&packet));

    avec(avcodec_open2(context, codec, NULL));

    if (output_filepath == nullptr) {
        println(stderr, "Error: Output filepath is not provided. Use `--output` flag.");
        usage(stderr);
        exit(1);
    }

    FILE *output_stream = fail_if_null(
        fopen(output_filepath, "wb"),
        "Could not open ", output_filepath);
    defer(fclose(output_stream));

    AVFrame *frame = fail_if_null(
        av_frame_alloc(),
        "Could not allocate video frame");
    defer(av_frame_free(&frame));

    frame->format = context->pix_fmt;
    frame->width  = context->width;
    frame->height = context->height;

    avec(av_frame_get_buffer(frame, 32));
    // FFMPEG INIT STOP //////////////////////////////

    // TODO(#35): log is not retrived directly from the Twitch API
    //   See https://github.com/PetterKraabol/Twitch-Chat-Downloader
    if (log_filepath == nullptr) {
        println(stderr, "Input log file is not provided");
        usage(stderr);
        exit(1);
    }
    auto input = read_file_as_string_view(log_filepath);
    if (!input.has_value) {
        println(stderr, "Could not read file `", log_filepath, "`");
        abort();
    }
    while (input.unwrap.count > 0) {
        assert(messages_size < VODUS_MESSAGES_CAPACITY);
        String_View message = input.unwrap.chop_by_delim('\n');
        messages[messages_size].timestamp = (int) chop_timestamp(&message);
        messages[messages_size].nickname = chop_nickname(&message);
        messages[messages_size].message = message.trim();
        messages_size++;
    }
    messages_size = min(messages_size, messages_limit);
    std::sort(messages, messages + messages_size,
              [](const Message &m1, const Message &m2) {
                  return m1.timestamp < m2.timestamp;
              });

    Frame_Encoder encoder = {};
    encoder.frame = frame;
    encoder.context = context;
    encoder.packet = packet;
    encoder.output_stream = output_stream;

    {
        clock_t begin = clock();
        sample_chat_log_animation(face, &encoder, &emote_cache, params);
        println(stdout, "Rendering took ", (float) (clock() - begin) / (float) CLOCKS_PER_SEC, " seconds");
    }

    encode_avframe(context, NULL, packet, output_stream);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), output_stream);

    return 0;
}
