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
    const float total_t = (float) messages[messages_size - 1].timestamp / 1000.0f + TRAILING_BUFFER_SEC;
    for (; message_end < messages_size; ++frame_index) {
        if (message_cooldown <= 0.0f) {
            message_buffer->push(messages[message_end], face, emote_cache, params);

            message_end += 1;
            auto t1 = (float) messages[message_end - 1].timestamp / 1000.0f;
            auto t2 = (float) messages[message_end].timestamp / 1000.0f;
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

    Emote_Cache emote_cache = { };
    emote_cache.populate_from_file("./mapping.csv", params.font_size);

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

    Message *messages = nullptr;
    size_t messages_size = parse_messages_from_string_view(input.unwrap, &messages, params);
    defer(delete[] messages);

    Encoder encoder = {};
    switch (params.output_type) {
    case Output_Type::Video: {
        encoder.context = new_avencoder_context(params, output_filepath);
        encoder.encode_func = (Encode_Func) &avencoder_encode;
    } break;

    case Output_Type::PNG: {
        encoder.context = new PNGEncoder_Context {cstr_as_string_view(output_filepath)};
        encoder.encode_func = (Encode_Func) &pngencoder_encode;
    } break;
    }

    {
        clock_t begin = clock();
        sample_chat_log_animation(messages, messages_size, face, encoder, &emote_cache, params);
        println(stdout, "Rendering took ", (float) (clock() - begin) / (float) CLOCKS_PER_SEC, " seconds");
    }

    switch (params.output_type) {
    case Output_Type::Video: {
        AVEncoder_Context *context = (AVEncoder_Context *) encoder.context;

        encode_avframe(context->context, NULL, context->packet, context->output_stream);

        uint8_t endcode[] = { 0, 0, 1, 0xb7 };
        if (context->codec->id == AV_CODEC_ID_MPEG1VIDEO || context->codec->id == AV_CODEC_ID_MPEG2VIDEO) {
            fwrite(endcode, 1, sizeof(endcode), context->output_stream);
        }

        free_avencoder_context(context);
    } break;

    case Output_Type::PNG: {
        delete ((PNGEncoder_Context*) encoder.context);
    } break;
    }

    return 0;
}
