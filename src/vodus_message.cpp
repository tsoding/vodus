struct Message
{
    uint64_t timestamp;
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
            auto maybe_emote = emote_cache->emote_by_name(word, params.font_size);

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

            // TODO(#112): no option to not separate consecutive emotes for better looking combos
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

void print1(FILE *stream, Message message)
{
    print(
        stream,
        "Message { ",
        ".timestamp = ", message.timestamp, ", ",
        ".nickname = ", message.nickname, ", ",
        ".message = ", message.message,
        "}");
}

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

String_View get_substring_view_by_name(pcre2_code *re,
                                       pcre2_match_data *match_data,
                                       const char *name,
                                       String_View subject)
{
    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    int index = pcre2_substring_number_from_name(re, (PCRE2_SPTR) name);
    const char* substring_start = subject.data + ovector[2*index];
    size_t substring_length = ovector[2*index+1] - ovector[2*index];
    return {substring_length, substring_start};
}

uint64_t get_timestamp_from_match_data(pcre2_code *re,
                                       pcre2_match_data *match_data,
                                       String_View subject)
{
    String_View hours = get_substring_view_by_name(re, match_data, "hours", subject);
    String_View minutes = get_substring_view_by_name(re, match_data, "minutes", subject);
    String_View seconds = get_substring_view_by_name(re, match_data, "seconds", subject);
    String_View mseconds = get_substring_view_by_name(re, match_data, "milliseconds", subject);

    uint64_t mseconds_value = 0;
    for (size_t i = 0; i < 3; ++i) {
        uint64_t x = 0;
        if (mseconds.count > 0) {
            x = *mseconds.data - '0';
            mseconds.chop(1);
        }
        mseconds_value = mseconds_value * 10 + x;
    }

    const uint64_t timestamp =
        (hours.as_integer<uint64_t>().unwrap * 60 * 60 +
         minutes.as_integer<uint64_t>().unwrap * 60 +
         seconds.as_integer<uint64_t>().unwrap) * 1000 +
        mseconds_value;

    return timestamp;
}

size_t parse_messages_from_string_view(String_View input, Message **messages, Video_Params params, const char *input_filepath)
{
    int errorcode = 0;
    PCRE2_SIZE erroroffset = 0;
    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR) params.message_regex.data,
        params.message_regex.count, 0,
        &errorcode,
        &erroroffset,
        NULL);

    if (re == NULL) {
        PCRE2_UCHAR buffer[256];
        pcre2_get_error_message(errorcode, buffer, sizeof(buffer));
        // TODO(#136): better PCRE2 compilation errors
        printf("PCRE2 compilation of message_regex failed at offset %d: %s\n", (int)erroroffset, buffer);
        exit(1);
    }
    defer(pcre2_code_free(re));

    pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(re, NULL);
    defer(pcre2_match_data_free(match_data));

    size_t expected_messages_size = input.count_chars('\n') + 1;
    if (params.messages_limit.has_value) {
        expected_messages_size = min(expected_messages_size, params.messages_limit.unwrap);
    }

    *messages = new Message[expected_messages_size];

    size_t messages_size = 0;
    for (size_t line_number = 1; input.count > 0 && messages_size < expected_messages_size; ++line_number) {
        String_View message = input.chop_by_delim('\n');

        int rc = pcre2_match(
            re,                           /* the compiled pattern */
            (PCRE2_SPTR) message.data,    /* the subject string */
            message.count,                /* the length of the subject */
            0,                            /* start at offset 0 in the subject */
            0,                            /* default options */
            match_data,                   /* block for storing the result */
            NULL);                        /* use default match context */

        if (rc < 0) {
            print(stderr, input_filepath, ":", line_number, ": ");

            switch(rc) {
            case PCRE2_ERROR_NOMATCH:
                println(stderr, "message_regex did not match this line");
                break;
            default:
                println(stderr, "Matching error ", rc);
                break;
            }

            exit(1);
        }

        (*messages)[messages_size].timestamp = get_timestamp_from_match_data(re, match_data, message);
        (*messages)[messages_size].nickname = get_substring_view_by_name(re, match_data, "nickname", message);
        (*messages)[messages_size].message = get_substring_view_by_name(re, match_data, "message", message).trim();
        messages_size++;
    }

    std::sort(*messages, *messages + messages_size,
              [](const Message &m1, const Message &m2) {
                  return m1.timestamp < m2.timestamp;
              });
    return messages_size;
}
