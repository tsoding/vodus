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
                    Emote_Cache *emote_cache)
{
    assert(emote_cache);

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
        auto maybe_emote = emote_cache->emote_by_name(word);

        // TODO(#23): Twitch emotes are not rendered
        if (maybe_emote.has_value) {
            auto emote = maybe_emote.unwrap;

            const float emote_ratio = (float) emote.width() / emote.height();
            const int emote_height = VODUS_FONT_SIZE;
            const int emote_width = floorf(emote_height * emote_ratio);

            if (*x + emote_width >= (int)surface.width) {
                *x = 0;
                // TODO(#31): the size of the font in word wrapping should be taken from the face itself
                //   Right now font size is hardcoded.
                //   Grep for `@word-wrap-face` to find all of the hardcoded places
                *y += VODUS_FONT_SIZE;
            }

            emote.slap_onto_image32(surface,
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
                Emote_Cache *emote_cache)
{
    fill_image32_with_color(surface, {0, 0, 0, 255});
    const int FONT_HEIGHT = VODUS_FONT_SIZE;
    const int CHAT_PADDING = 0;
    int text_y = FONT_HEIGHT + CHAT_PADDING;
    for (size_t i = message_begin; i < message_end; ++i) {
        int text_x = 0;
        render_message(surface, face, messages[i], &text_x, &text_y, emote_cache);
        text_y += FONT_HEIGHT + CHAT_PADDING;
    }
    return text_y > (int)surface.height;
}

void sample_chat_log_animation(FT_Face face, Encode_Frame encode_frame, Emote_Cache *emote_cache)
{
    assert(emote_cache);

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
        while (render_log(surface, face, message_begin, message_end, emote_cache) &&
               message_begin < messages_size) {
            message_begin++;
        }
        encode_frame(surface, frame_index);

        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
    }

    const size_t TRAILING_BUFFER_SEC = 2;
    for (size_t i = 0; i < TRAILING_BUFFER_SEC * VODUS_FPS; ++i, ++frame_index) {
        while (render_log(surface, face, message_begin, message_end, emote_cache) &&
               message_begin < messages_size) {
            message_begin++;
        }
        emote_cache->update_gifs(VODUS_DELTA_TIME_SEC);
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
    println(stream, "    --font <filepath>         Path to the Font face file");
    println(stream, "    --limit <number>          Limit the amout of messages to render");
}

#include "tzozen.h"

const size_t JSON_MEMORY_BUFFER_CAPACITY = 10 * MEGA;
static uint8_t json_memory_buffer[JSON_MEMORY_BUFFER_CAPACITY];
String_Buffer curl_buffer = {};

static inline
void expect_json_type(Json_Value value, Json_Type type)
{
    if (value.type != type) {
        fprintf(stderr,
                "Expected %s, but got %s\n",
                json_type_as_cstr(type),
                json_type_as_cstr(value.type));
        abort();
    }
}

const size_t PATH_BUFFER_SIZE = 256;

struct Curl_Download
{
    String_Buffer<PATH_BUFFER_SIZE> src_url;
    String_Buffer<PATH_BUFFER_SIZE> dst_path;
};

const size_t CURL_DOWNLOAD_QUEUE_CAP = 256;
struct Curl_Download_Queue
{
    Curl_Download downloads[CURL_DOWNLOAD_QUEUE_CAP];
    size_t begin;
    size_t size;
    pthread_mutex_t mutex;
    pthread_cond_t cond_full;
    pthread_cond_t cond_empty;
    int is_done = 0;

    void done()
    {
        pthread_mutex_lock(&mutex);
        is_done = 1;
        pthread_mutex_unlock(&mutex);
    }

    void push(Curl_Download download)
    {
        pthread_mutex_lock(&mutex);
        while (size >= CURL_DOWNLOAD_QUEUE_CAP) {
            pthread_cond_wait(&cond_full, &mutex);
        }

        downloads[(begin + size) % CURL_DOWNLOAD_QUEUE_CAP] = download;
        size += 1;

        pthread_cond_signal(&cond_empty);
        pthread_mutex_unlock(&mutex);
    }

    int pop(Curl_Download *download)
    {
        pthread_mutex_lock(&mutex);

        while (size == 0) {
            if (is_done) {
                pthread_cond_signal(&cond_empty);
                pthread_mutex_unlock(&mutex);
                return 0;
            }

            pthread_cond_wait(&cond_empty, &mutex);
        }

        *download = downloads[begin];
        begin = (begin + 1) % CURL_DOWNLOAD_QUEUE_CAP;
        size -= 1;

        pthread_cond_signal(&cond_full);
        pthread_mutex_unlock(&mutex);

        return 1;
    }

    void error(const char *message)
    {
        assert(0 && "TODO: Curl_Download_Queue::error() is not implemented");
    }
};

void *curl_download_all_from_queue(void *arg)
{
    Curl_Download_Queue *queue = (Curl_Download_Queue *) arg;
    CURL *curl = curl_easy_init();
    if (!curl) {
        println(stderr, "CURL pooped itself: could not initialize the state");
        abort();
    }
    defer(curl_easy_cleanup(curl));

    Curl_Download download = {};
    while (queue->pop(&download)) {
        auto res = curl_download_file_to(curl, download.src_url.data, download.dst_path.data);
        if (res != CURLE_OK) {
            queue->error(curl_easy_strerror(res));
            return NULL;
        }

        println(stdout, "Downloading ", download.src_url, " to ", download.dst_path);
    }

    return queue;
}

void append_bttv_mapping(CURL *curl, const char *emotes_url, Curl_Download_Queue *queue)
{
    curl_buffer.clean();

    auto res = curl_perform_to_string_buffer(curl, emotes_url, &curl_buffer);
    if (res != CURLE_OK) {
        println(stderr, "curl_perform_to_string_buffer() failed: ",
                curl_easy_strerror(res));
        abort();
    }

    Memory memory = {
        .capacity = JSON_MEMORY_BUFFER_CAPACITY,
        .buffer = json_memory_buffer
    };

    String source = {
        .len = curl_buffer.size,
        .data = curl_buffer.data
    };

    Json_Result result = parse_json_value(&memory, source);
    if (result.is_error) {
        print_json_error(stdout, result, source, emotes_url);
        abort();
    }

    expect_json_type(result.value, JSON_OBJECT);
    auto emotes = json_object_value_by_key(result.value.object, SLT("emotes"));
    expect_json_type(emotes, JSON_ARRAY);

    FOR_ARRAY_JSON(emote, emotes.array, {
        expect_json_type(emote, JSON_OBJECT);

        auto emote_id = json_object_value_by_key(emote.object, SLT("id"));
        expect_json_type(emote_id, JSON_STRING);

        auto emote_code = json_object_value_by_key(emote.object, SLT("code"));
        expect_json_type(emote_code, JSON_STRING);

        auto emote_imageType = json_object_value_by_key(emote.object, SLT("imageType"));
        expect_json_type(emote_imageType, JSON_STRING);

        String_Buffer<PATH_BUFFER_SIZE> path_buffer = {};
        String_Buffer<PATH_BUFFER_SIZE> url_buffer = {};

        path_buffer.write("emotes/"); // TODO: check for path_buffer.write return 09
        path_buffer.write(emote_id.string.data, emote_id.string.len);
        path_buffer.write(".");
        path_buffer.write(emote_imageType.string.data, emote_imageType.string.len);

        url_buffer.write("https://cdn.betterttv.net/emote/");
        url_buffer.write(emote_id.string.data, emote_id.string.len);
        url_buffer.write("/3x");

        queue->push(Curl_Download { url_buffer, path_buffer });
        // TODO: mapping file is not created anymore
    });
}

Curl_Download_Queue curl_download_queue;

const size_t THREAD_COUNT = 20;
pthread_t download_threads[THREAD_COUNT];

int main(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    defer(curl_global_cleanup());

    CURL *curl = curl_easy_init();
    if (!curl) {
        println(stderr, "CURL pooped itself: could not initialize the state");
        abort();
    }
    defer(curl_easy_cleanup(curl));

    const char *EMOTE_CACHE_DIRECTORY = "./emotes/";
    const char *MAPPING_FILE = "./mapping.csv";

    FILE *mapping = fopen(MAPPING_FILE, "w");
    if (mapping == NULL) {
        println(stderr, "Could not open file ", MAPPING_FILE);
    }
    defer(fclose(mapping));

    create_directory_if_not_exists(EMOTE_CACHE_DIRECTORY);

    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        int err = pthread_create(&download_threads[i], NULL,
                                 curl_download_all_from_queue,
                                 &curl_download_queue);
        if (err != 0) {
            println(stderr, "Could not create thread: ", strerror(err));
            abort();
        }
    }

    append_bttv_mapping(curl, "https://api.betterttv.net/2/emotes", &curl_download_queue);
    append_bttv_mapping(curl, "https://api.betterttv.net/2/channels/tsoding", &curl_download_queue);
    curl_download_queue.done();

    for (size_t i = 0; i < THREAD_COUNT; ++i) {
        void *retval = 0;
        int err = pthread_join(download_threads[i], &retval);
        if (err != 0) {
            println(stderr, "Could not join a thread: ", strerror(err));
            // NOTE: just skip the thread and try to join the rest of them
        }
    }

    // TODO: handle any errors that might've happened during the download in any of the threads

    return 0;
}

int main_(int argc, char *argv[])
{
    const char *log_filepath = nullptr;
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

    // TODO(#44): BTTV mapping is not auto populated from the BTTV API
    // TODO(#45): FFZ mapping is not auto populated from the FFZ API
    Emote_Cache emote_cache = { };
    auto mapping_csv = file_as_string_view("./mapping.csv");
    while (mapping_csv.count > 0) {
        auto line = mapping_csv.chop_by_delim('\n');
        auto name = line.chop_by_delim(',');
        emote_cache.add_mapping(name, line);
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

    sample_chat_log_animation(face, encode_frame, &emote_cache);

    encode_avframe(context, NULL, packet, output_stream);

    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    if (codec->id == AV_CODEC_ID_MPEG1VIDEO || codec->id == AV_CODEC_ID_MPEG2VIDEO)
        fwrite(endcode, 1, sizeof(endcode), output_stream);

    return 0;
}
