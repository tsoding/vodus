using Timestamp = uint64_t;

struct Message
{
    Timestamp timestamp;
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

Timestamp chop_timestamp(const char *input_filepath,
                         size_t line_number,
                         String_View *input,
                         String_View origin_input)
{
    auto syntax_panic = [&] (auto ...args) {
        panic(input_filepath, ":", line_number, ":", input->data - origin_input.data + 1, ": ",
              args...);
    };

    auto unwrap_or_syntax_panic = [&] (auto maybe, auto ...args) {
        if (!maybe.has_value) {
            syntax_panic(args...);
        }

        return maybe.unwrap;
    };

    auto is_digit = [](char x) -> bool {
        return isdigit(x);
    };

    auto expect_char = [&](char x) {
        if (input->count == 0 || *input->data != x) {
            syntax_panic("Expected `", x, "`");
        }
        input->chop(1);
    };

    auto expect_number = [&]() -> uint64_t {
        return unwrap_or_syntax_panic(
            input->chop_while(is_digit).as_integer<uint64_t>(),
            "Expected number");
    };

    *input = input->trim_begin();

    expect_char('[');
    auto hours = expect_number();
    expect_char(':');
    auto minutes = expect_number();
    expect_char(':');
    auto seconds = expect_number();

    Timestamp mseconds = 0;
    if (input->count > 0 && *input->data == '.') {
        input->chop(1);
        auto mseconds_str = input->chop_while(is_digit);
        for (size_t i = 0; i < 3; ++i) {
            if (i < mseconds_str.count) {
                mseconds = mseconds * 10 + mseconds_str.data[i] - '0';
            } else {
                mseconds *= 10;
            }
        }
    }

    expect_char(']');

    return (hours * 60 * 60 + minutes * 60 + seconds) * 1000 + mseconds;
}

String_View chop_nickname(const char *input_filepath,
                          size_t line_number,
                          String_View *input,
                          String_View origin_input)
{
    auto syntax_panic = [&] (auto ...args) {
        panic(input_filepath, ":", line_number, ":", input->data - origin_input.data + 1, ": ",
              args...);
    };

    auto expect_char = [&](char x) {
        if (input->count == 0 || *input->data != x) {
            syntax_panic("Expected `", x, "`");
        }
        input->chop(1);
    };

    *input = input->trim_begin();
    expect_char('<');
    auto nickname = input->chop_while([](char x) -> bool {
        return x != '>';
    });
    expect_char('>');

    return nickname;
}

String_View chop_message(String_View *input)
{
    return input->trim();
}

size_t parse_messages_from_string_view(String_View input,
                                       Message **messages,
                                       Video_Params params,
                                       const char *input_filepath)
{
    size_t expected_messages_size = input.count_chars('\n') + 1;
    if (params.messages_limit.has_value) {
        expected_messages_size = min(expected_messages_size, params.messages_limit.unwrap);
    }

    *messages = new Message[expected_messages_size];

    size_t messages_size = 0;
    for (size_t line_number = 1;
         input.count > 0 && messages_size < expected_messages_size;
         ++line_number)
    {
        String_View origin_message = input.chop_by_delim('\n');
        String_View message = origin_message;

        (*messages)[messages_size].timestamp = chop_timestamp(input_filepath, line_number, &message, origin_message);
        (*messages)[messages_size].nickname  = chop_nickname(input_filepath, line_number, &message, origin_message);
        (*messages)[messages_size].message   = chop_message(&message);
        messages_size++;
    }

    std::sort(*messages, *messages + messages_size,
              [](const Message &m1, const Message &m2) {
                  return m1.timestamp < m2.timestamp;
              });

    return messages_size;
}
