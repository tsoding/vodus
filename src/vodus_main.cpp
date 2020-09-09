void sample_chat_log_animation(Message *messages,
                               size_t messages_size,
                               FT_Face face,
                               Encoder encoder,
                               Emote_Cache *emote_cache,
                               Video_Params params)
{
    auto message_buffer = new Message_Buffer<1024>();
    defer(delete message_buffer);

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
            message_buffer->push(messages[message_end], face, emote_cache, params);

            message_end += 1;
            auto t1 = messages[message_end - 1].timestamp;
            auto t2 = messages[message_end].timestamp;
            message_cooldown = t2 - t1;
        }

        message_cooldown -= VODUS_DELTA_TIME_SEC;

        fill_image32_with_color(surface, params.background_color);
        message_buffer->render(surface, face, emote_cache, params);
        encoder.encode(surface, frame_index);

        t += VODUS_DELTA_TIME_SEC;
        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_buffer->update(VODUS_DELTA_TIME_SEC, face, emote_cache, params);

        print(stdout, "\rRendered ", (int) roundf(t), "/", (int) roundf(total_t), " seconds");
    }

    for (size_t i = 0; i < TRAILING_BUFFER_SEC * params.fps; ++i, ++frame_index) {
        fill_image32_with_color(surface, params.background_color);
        message_buffer->render(surface, face, emote_cache, params);

        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
        message_buffer->update(VODUS_DELTA_TIME_SEC, face, emote_cache, params);
        encoder.encode(surface, frame_index);

        t += VODUS_DELTA_TIME_SEC;
        print(stdout, "\rRendered ", (int) roundf(t), "/", (int) roundf(total_t), " seconds");
    }

    print(stdout, "\rRendered ", (int) roundf(total_t), "/", (int) roundf(total_t), " seconds");
    print(stdout, "\n");
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
    emote_cache.populate_from_file("./mapping.csv", params.font_size);

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

    Message *messages = new Message[VODUS_MESSAGES_CAPACITY];
    defer(delete[] messages);
    size_t messages_size = parse_messages_from_string_view(input.unwrap, messages, params);

    AVEncoder_Context avencoder_context ={};
    avencoder_context.frame = frame;
    avencoder_context.context = context;
    avencoder_context.packet = packet;
    avencoder_context.output_stream = output_stream;

    Encoder encoder = {&avencoder_context, (Encode_Func) avencoder_encode};

    {
        clock_t begin = clock();
        sample_chat_log_animation(messages, messages_size, face, encoder, &emote_cache, params);
        println(stdout, "Rendering took ", (float) (clock() - begin) / (float) CLOCKS_PER_SEC, " seconds");
    }

    encode_avframe(context, NULL, packet, output_stream);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), output_stream);

    return 0;
}
