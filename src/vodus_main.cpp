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

    /// returns amount of rows the message took
    int height(FT_Face face,
               Emote_Cache *emote_cache,
               Video_Params params)
    {
        Image32 dummy = {};
        dummy.width = params.width;
        return render(dummy, face, emote_cache, params, 0);
    }

    /// returns amount of rows the message took
    int render(Image32 surface, FT_Face face,
               Emote_Cache *emote_cache,
               Video_Params params,
               int row)
    {
        int result = 1;
        int pen_x = 0;
        int pen_y = (row + 1) * params.font_size;

        slap_text_onto_image32(surface,
                               face,
                               nickname,
                               params.nickname_color,
                               &pen_x, &pen_y);


        auto text = message.trim();
        auto text_color = params.text_color;
        const char *nick_text_sep = ": ";

        const auto slash_me = "/me"_sv;

        if (text.has_prefix(slash_me)) {
            text.chop(slash_me.count);
            text_color = params.nickname_color;
            nick_text_sep = " ";
        }

        slap_text_onto_image32(surface,
                               face,
                               nick_text_sep,
                               params.nickname_color,
                               &pen_x, &pen_y);

        while (text.count > 0) {
            auto word = text.chop_word().trim();
            auto maybe_emote = emote_cache->emote_by_name(word);

            // TODO(#23): Twitch emotes are not rendered
            if (maybe_emote.has_value) {
                auto emote = maybe_emote.unwrap;

                if (pen_x + emote.width() >= (int)surface.width) {
                    pen_x = 0;
                    pen_y += params.font_size;
                    // WRAPPED!
                    result += 1;
                }

                emote.slap_onto_image32(surface, pen_x, pen_y - emote.height());
                pen_x += emote.width();
            } else {
                if (slap_text_onto_image32_wrapped(surface,
                                                   face,
                                                   word,
                                                   text_color,
                                                   &pen_x, &pen_y,
                                                   params.font_size)) {
                    // WRAPPED!
                    result += 1;
                }
            }

            if (text.count > 0 &&
                slap_text_onto_image32_wrapped(surface,
                                               face,
                                               " ",
                                               text_color,
                                               &pen_x, &pen_y,
                                               params.font_size)) {
                // WRAPPED!
                result += 1;
            }
        }

        return result;
    }
};

Message messages[VODUS_MESSAGES_CAPACITY];
size_t messages_size = 0;

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
struct Message_Buffer
{
    Queue<Message, Capacity> message_queue;
    int height;
    int begin;

    // TODO(#74): message entering/leaving animation should be disablable
    // TODO(#75): entering/leaving animation should also animate alpha
    // TODO(#105): message animation is broken

    void normalize_queue(FT_Face face,
                         Emote_Cache *emote_cache,
                         Video_Params params)
    {
        while (message_queue.count > 0) {
            auto message_height = message_queue.first().height(face, emote_cache, params);
            if (message_height >= abs(begin)) return;
            message_queue.dequeue();
            begin += message_height;
            height -= message_height;
        }
    }

    void push(Message message,
              FT_Face face,
              Emote_Cache *emote_cache,
              Video_Params params)
    {
        const auto message_height = message.height(face, emote_cache, params);
        const int rows_count = params.height / params.font_size;

        if (begin + height + message_height > rows_count) {
            begin -= message_height;
        }

        message_queue.enqueue(message);
        height += message_height;
    }

    void update(float dt, FT_Face face, Emote_Cache *emote_cache, Video_Params params)
    {
        normalize_queue(face, emote_cache, params);
    }

    void render(Image32 surface,
                FT_Face face,
                Emote_Cache *emote_cache,
                Video_Params params)
    {
        int row = begin;
        for (size_t i = 0; i < message_queue.count; ++i) {
            row += message_queue[i].render(surface,
                                           face,
                                           emote_cache,
                                           params,
                                           row);
        }
    }
};

Message_Buffer<1024> message_buffer = {};

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
    for (; message_end < messages_size; ++frame_index) {
        if (message_cooldown <= 0.0f) {
            message_buffer.push(messages[message_end], face, emote_cache, params);

            message_end += 1;
            auto t1 = messages[message_end - 1].timestamp;
            auto t2 = messages[message_end].timestamp;
            message_cooldown = t2 - t1;
        }

        message_cooldown -= VODUS_DELTA_TIME_SEC;

        fill_image32_with_color(surface, params.background_color);
        message_buffer.render(surface, face, emote_cache, params);
        encoder->encode_frame(surface, frame_index);

        t += VODUS_DELTA_TIME_SEC;
        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_buffer.update(VODUS_DELTA_TIME_SEC, face, emote_cache, params);

        print(stdout, "\rRendered ", (int) roundf(t), "/", (int) roundf(total_t), " seconds");
    }

    for (size_t i = 0; i < TRAILING_BUFFER_SEC * params.fps; ++i, ++frame_index) {
        fill_image32_with_color(surface, params.background_color);
        message_buffer.render(surface, face, emote_cache, params);

        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_buffer.update(VODUS_DELTA_TIME_SEC, face, emote_cache, params);
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

int main(int argc, char *argv[])
{
    Args args = {argc, argv};
    args.pop();                 // skip program name;

    if (args.empty()) {
        println(stderr, "[ERROR] Input filename is not provided");
        usage(stderr);
        abort();
    }
    const char *input_filepath = args.pop();

    if (args.empty()) {
        println(stderr, "[ERROR] Output filename is not provided");
        usage(stderr);
        abort();
    }
    const char *output_filepath = args.pop();

    Video_Params params = default_video_params();

    patch_video_params_from_args(&params, &args);

    println(stdout, "params = ", params);

    if (params.width % 2 != 0 || params.height % 2 != 0) {
        println(stderr, "Error: resolution must be multiple of two!");
        exit(1);
    }

    FT_Library library;
    auto error = FT_Init_FreeType(&library);
    if (error) {
        println(stderr, "Could not initialize FreeType2");
        exit(1);
    }

    const char *face_filepath = string_view_as_cstr(params.font);
    defer(free((void*) face_filepath));

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

    // TODO(#92): loading emotes step is too slow
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
    if (input_filepath == nullptr) {
        println(stderr, "Input log file is not provided");
        usage(stderr);
        exit(1);
    }
    auto input = read_file_as_string_view(input_filepath);
    if (!input.has_value) {
        println(stderr, "Could not read file `", input_filepath, "`");
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
    messages_size = min(messages_size, params.messages_limit);
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
