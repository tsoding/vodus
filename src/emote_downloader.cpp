#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define CURL_STRICTER
#include <curl/curl.h>
#define TZOZEN_IMPLEMENTATION
#include "./tzozen.h"
#include "./aids.hpp"
using namespace aids;

template <typename T, size_t Capacity>
struct Fixed_Array
{
    size_t size;
    T elements[Capacity];

    void push(T x)
    {
        assert(size < Capacity);
        elements[size++] = x;
    }
};

const size_t DEFAULT_STRING_BUFFER_CAPACITY = 20 * 1024 * 1024;

template <size_t Capacity = DEFAULT_STRING_BUFFER_CAPACITY>
struct Fixed_Buffer
{
    size_t size;
    char data[Capacity + 1];

    size_t write(const char *data, size_t size)
    {
        if (this->size + size > Capacity) return 0;
        memcpy(this->data + this->size, data, size);
        this->size += size;
        return size;
    }

    size_t write(const char *cstr)
    {
        return write(cstr, strlen(cstr));
    }

    size_t write(size_t x)
    {
        const auto n = snprintf(data + size, Capacity - size, "%ld", x);
        this->size += n;
        return n;
    }

    void clean()
    {
        memset(this->data, 0, Capacity + 1);
        this->size = 0;
    }
};

template <size_t Capacity>
void print1(FILE *stream, Fixed_Buffer<Capacity> buffer)
{
    fwrite(buffer.data, 1, buffer.size, stream);
}

size_t curl_string_buffer_write_callback(char *ptr,
                                         size_t size,
                                         size_t nmemb,
                                         Fixed_Buffer<> *string_buffer)
{
    return string_buffer->write(ptr, size * nmemb);
}

CURLcode curl_download_file_to(CURL *curl, const char *url, const char *filepath)
{
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        println(stderr, "Could not open file `", filepath, "`");
        abort();
    }
    defer(fclose(file));

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    return curl_easy_perform(curl);
}

template <size_t Capacity = DEFAULT_STRING_BUFFER_CAPACITY>
CURLcode curl_perform_to_string_buffer(CURL *curl,
                                       const char *url,
                                       Fixed_Buffer<Capacity> *string_buffer)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_string_buffer_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, string_buffer);
    return curl_easy_perform(curl);
}

const size_t JSON_MEMORY_BUFFER_CAPACITY = 10 * MEGA;
static uint8_t json_memory_buffer[JSON_MEMORY_BUFFER_CAPACITY];
Fixed_Buffer curl_buffer = {};

static inline
void expect_json_type(Json_Value value, Json_Type type)
{
    if (value.type != type) {
        println(stderr,
                "Expected ", json_type_as_cstr(type), ", but got ",
                json_type_as_cstr(value.type));
        abort();
    }
}

const size_t PATH_BUFFER_SIZE = 1024;
const size_t MAX_PARALLEL = 20;

struct Curl_Download
{
    Fixed_Buffer<PATH_BUFFER_SIZE> url;
    Fixed_Buffer<PATH_BUFFER_SIZE> file;
};

void add_download_to_multi_handle(Curl_Download download, CURLM *cm)
{
    FILE *file = fopen(download.file.data, "wb");
    CURL *eh = curl_easy_init();
    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(eh, CURLOPT_URL, download.url.data);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, file);
    curl_multi_add_handle(cm, eh);
}

void print1(FILE *stream, Curl_Download download)
{
    print(stream, "Curl_Download { .url = \"", download.url, "\", .file = \"", download.file, "\" }");
}

void print1(FILE *stream, String tzozen_string)
{
    fwrite(tzozen_string.data, 1, tzozen_string.len, stream);
}

void append_ffz_set(CURL *curl,
                    Json_Object set,
                    FILE *mapping,
                    Fixed_Array<Curl_Download, 1024> *downloads)
{
    auto emoticons = json_object_value_by_key(set, SLT("emoticons"));
    expect_json_type(emoticons, JSON_ARRAY);

    FOR_JSON(Json_Array, emoticon, emoticons.array) {
        expect_json_type(emoticon->value, JSON_OBJECT);
        auto urls = json_object_value_by_key(emoticon->value.object, SLT("urls"));
        expect_json_type(urls, JSON_OBJECT);

        if (urls.object.end) {
            auto url = urls.object.end->value;
            expect_json_type(url, JSON_STRING);
            auto emote_id = json_object_value_by_key(emoticon->value.object, SLT("id"));
            expect_json_type(emote_id, JSON_NUMBER);
            auto name = json_object_value_by_key(emoticon->value.object, SLT("name"));
            expect_json_type(name, JSON_STRING);

            Curl_Download download = {};

            download.file.write("emotes/emote-");
            download.file.write(downloads->size);
            download.file.write(".png");
            download.url.write("https:");
            download.url.write(url.string.data, url.string.len);

            println(mapping, name.string, ",", download.file);
            downloads->push(download);
        }
    }
}

Json_Value curl_perform_to_json_value(CURL *curl, const char *url)
{
    curl_buffer.clean();
    auto res = curl_perform_to_string_buffer(curl, url, &curl_buffer);
    if (res != CURLE_OK) {
        println(stderr, "curl_perform_to_string_buffer() failed: ",
                curl_easy_strerror(res));
        abort();
    }

    Memory memory = {};
    memory.capacity = JSON_MEMORY_BUFFER_CAPACITY;
    memory.buffer = json_memory_buffer;

    String source = {
        curl_buffer.size,
        curl_buffer.data
    };

    Json_Result result = parse_json_value(&memory, source);
    if (result.is_error) {
        print_json_error(stdout, result, source, url);
        abort();
    }

    return result.value;
}

void append_room_ffz_mapping(CURL *curl,
                             const char *emotes_url,
                             FILE *mapping,
                             Fixed_Array<Curl_Download, 1024> *downloads)
{
    auto result = curl_perform_to_json_value(curl, emotes_url);
    expect_json_type(result, JSON_OBJECT);

    auto room = json_object_value_by_key(result.object, SLT("room"));
    expect_json_type(room, JSON_OBJECT);

    auto sets = json_object_value_by_key(result.object, SLT("sets"));
    expect_json_type(sets, JSON_OBJECT);

    auto set_id = json_object_value_by_key(room.object, SLT("set"));
    expect_json_type(set_id, JSON_NUMBER);

    auto set = json_object_value_by_key(sets.object, set_id.number.integer);
    expect_json_type(set, JSON_OBJECT);

    append_ffz_set(curl, set.object, mapping, downloads);
}

void append_global_ffz_mapping(CURL *curl,
                               FILE *mapping,
                               Fixed_Array<Curl_Download, 1024> *downloads)
{
    const char *const emotes_url = "https://api.frankerfacez.com/v1/set/global";
    auto result = curl_perform_to_json_value(curl, emotes_url);

    expect_json_type(result, JSON_OBJECT);
    auto default_sets = json_object_value_by_key(result.object, SLT("default_sets"));
    expect_json_type(default_sets, JSON_ARRAY);
    auto sets = json_object_value_by_key(result.object, SLT("sets"));
    expect_json_type(sets, JSON_OBJECT);

    FOR_JSON(Json_Array, set_id, default_sets.array) {
        expect_json_type(set_id->value, JSON_NUMBER);
        auto set = json_object_value_by_key(sets.object, set_id->value.number.integer);
        expect_json_type(set, JSON_OBJECT);
        append_ffz_set(curl, set.object, mapping, downloads);
    }
}

void append_bttv_emotes_array(Json_Array emotes_array,
                              FILE *mapping,
                              Fixed_Array<Curl_Download, 1024> *downloads)
{
    FOR_JSON (Json_Array, emote, emotes_array) {
        expect_json_type(emote->value, JSON_OBJECT);

        auto emote_id = json_object_value_by_key(emote->value.object, SLT("id"));
        expect_json_type(emote_id, JSON_STRING);

        auto emote_code = json_object_value_by_key(emote->value.object, SLT("code"));
        expect_json_type(emote_code, JSON_STRING);

        auto emote_imageType = json_object_value_by_key(emote->value.object, SLT("imageType"));
        expect_json_type(emote_imageType, JSON_STRING);

        Curl_Download download = {};

        download.file.write("emotes/emote-");
        download.file.write(downloads->size);
        download.file.write(".");
        download.file.write(emote_imageType.string.data, emote_imageType.string.len);

        download.url.write("https://cdn.betterttv.net/emote/");
        download.url.write(emote_id.string.data, emote_id.string.len);
        download.url.write("/3x");

        println(mapping, emote_code.string, ",", download.file);
        downloads->push(download);
    }
}

void append_global_bttv_mapping(CURL *curl,
                                const char *emotes_url,
                                FILE *mapping,
                                Fixed_Array<Curl_Download, 1024> *downloads)
{
    curl_buffer.clean();

    auto res = curl_perform_to_string_buffer(curl, emotes_url, &curl_buffer);
    if (res != CURLE_OK) {
        println(stderr, "curl_perform_to_string_buffer() failed: ",
                curl_easy_strerror(res));
        abort();
    }

    Memory memory = {};
    memory.capacity = JSON_MEMORY_BUFFER_CAPACITY;
    memory.buffer = json_memory_buffer;

    String source = {
        curl_buffer.size,
        curl_buffer.data
    };

    Json_Result result = parse_json_value(&memory, source);
    if (result.is_error) {
        print_json_error(stdout, result, source, emotes_url);
        abort();
    }

    expect_json_type(result.value, JSON_ARRAY);
    append_bttv_emotes_array(result.value.array, mapping, downloads);
}

void append_channel_twitch_mapping(CURL *curl,
                                   const char *emotes_url,
                                   FILE *mapping,
                                   Fixed_Array<Curl_Download, 1024> *downloads)
{
    curl_buffer.clean();

    auto res = curl_perform_to_string_buffer(curl, emotes_url, &curl_buffer);
    if (res != CURLE_OK) {
        println(stderr, "curl_perform_to_string_buffer() failed: ",
                curl_easy_strerror(res));
        abort();
    }

    Memory memory = {};
    memory.capacity = JSON_MEMORY_BUFFER_CAPACITY;
    memory.buffer = json_memory_buffer;

    String source = {
        curl_buffer.size,
        curl_buffer.data
    };

    Json_Result result = parse_json_value(&memory, source);
    if (result.is_error) {
        print_json_error(stdout, result, source, emotes_url);
        abort();
    }

    expect_json_type(result.value, JSON_OBJECT);
    auto channel_emotes = json_object_value_by_key(result.value.object, SLT("emotes"));
    expect_json_type(channel_emotes, JSON_ARRAY);

    FOR_JSON (Json_Array, emote, channel_emotes.array) {
        expect_json_type(emote->value, JSON_OBJECT);
        auto emote_code = json_object_value_by_key(emote->value.object, SLT("code"));
        expect_json_type(emote_code, JSON_STRING);
        auto emote_id = json_object_value_by_key(emote->value.object, SLT("id"));
        expect_json_type(emote_id, JSON_NUMBER);

        Curl_Download download = {};

        download.file.write("emotes/emote-");
        download.file.write(downloads->size);
        download.file.write(".png");

        download.url.write("https://static-cdn.jtvnw.net/emoticons/v1/");
        download.url.write(emote_id.string.data, emote_id.string.len);
        download.url.write("/3.0");

        println(mapping, emote_code.string, ",", download.file);
        downloads->push(download);
    }
}

void append_channel_bttv_mapping(CURL *curl,
                                 const char *emotes_url,
                                 FILE *mapping,
                                 Fixed_Array<Curl_Download, 1024> *downloads)
{
    curl_buffer.clean();

    auto res = curl_perform_to_string_buffer(curl, emotes_url, &curl_buffer);
    if (res != CURLE_OK) {
        println(stderr, "curl_perform_to_string_buffer() failed: ",
                curl_easy_strerror(res));
        abort();
    }

    Memory memory = {};
    memory.capacity = JSON_MEMORY_BUFFER_CAPACITY;
    memory.buffer = json_memory_buffer;

    String source = {
        curl_buffer.size,
        curl_buffer.data
    };

    Json_Result result = parse_json_value(&memory, source);
    if (result.is_error) {
        print_json_error(stdout, result, source, emotes_url);
        abort();
    }

    expect_json_type(result.value, JSON_OBJECT);

    auto channel_emotes = json_object_value_by_key(result.value.object, SLT("channelEmotes"));
    expect_json_type(channel_emotes, JSON_ARRAY);
    append_bttv_emotes_array(channel_emotes.array, mapping, downloads);

    auto shared_emotes = json_object_value_by_key(result.value.object, SLT("sharedEmotes"));
    expect_json_type(shared_emotes, JSON_ARRAY);
    append_bttv_emotes_array(shared_emotes.array, mapping, downloads);
}

bool is_sep(char x)
{
#ifdef _WIN32
    return x == '/' || x == '\\';
#else
    return x == '/';
#endif
}

struct Path
{
    String_View as_string_view;

    Path parent()
    {
        auto p = as_string_view;

        while (p.count > 0 && is_sep(*(p.data + p.count - 1))) {
            p.chop_back(1);
        }

        while (p.count > 0 && !is_sep(*(p.data + p.count - 1))) {
            p.chop_back(1);
        }

        return p.count == 0 ? Path {as_string_view} : Path {p};
    }

    void copy_to(char *buffer, size_t size)
    {
        memcpy(buffer, as_string_view.data,
               min(size, as_string_view.count));
    }
};

void print1(FILE *stream, Path path)
{
    print1(stream, path.as_string_view);
}

bool path_exists(Path path)
{
    char pathname[256 + 1] = {};
    path.copy_to(pathname, sizeof(pathname) - 1);
#ifdef _WIN32
    struct _stat statbuf = {};
    return _stat(pathname, &statbuf) == 0;
#else
    struct stat statbuf = {};
    return stat(pathname, &statbuf) == 0;
#endif
}

int create_directory(Path dirpath)
{
    char pathname[256 + 1] = {};
    dirpath.copy_to(pathname, sizeof(pathname) - 1);
#ifdef _WIN32
    return _mkdir(pathname);
#else
    return mkdir(pathname, 0755);
#endif
}

void create_all_directories(Path dirpath)
{
    while (!path_exists(dirpath)) {
        create_all_directories(dirpath.parent());
        create_directory(dirpath);
    }
}

Fixed_Array<Curl_Download, 1024> downloads;

void usage(FILE *stream)
{
    println(stdout, "./emote_downloader <channel-name> <channel-id>");
}

int main(int argc, char **argv)
{
    Args args = {argc, argv};
    args.shift();               // skip program name

    if (args.empty()) {
        usage(stderr);
        panic("ERROR: channel-name is not provided");
    }
    auto channel_name = args.shift();

    if (args.empty()) {
        usage(stderr);
        panic("ERROR: channel-id is not provided");
    }
    auto channel_id = args.shift();

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

    create_directory(Path {cstr_as_string_view(EMOTE_CACHE_DIRECTORY)});

    CURLM *cm = curl_multi_init();
    defer(curl_multi_cleanup(cm));

    curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, MAX_PARALLEL);

    char channel_url_buffer[1024];
    String_Buffer channel_url = {sizeof(channel_url_buffer), channel_url_buffer};


    // TODO(#152): emote_downloader does not download global Twitch emotes
    sprint(&channel_url, "https://api.twitchemotes.com/api/v4/channels/", channel_id);
    append_channel_twitch_mapping(curl, channel_url.data, mapping, &downloads);
    channel_url.size = 0;

    append_global_bttv_mapping(curl, "https://api.betterttv.net/3/cached/emotes/global", mapping, &downloads);

    sprint(&channel_url, "https://api.betterttv.net/3/cached/users/twitch/", channel_id);
    append_channel_bttv_mapping(curl, channel_url.data, mapping, &downloads);
    channel_url.size = 0;

    append_global_ffz_mapping(curl, mapping, &downloads);

    sprint(&channel_url, "https://api.frankerfacez.com/v1/room/", channel_name);
    append_room_ffz_mapping(curl, channel_url.data, mapping, &downloads);
    channel_url.size = 0;

    size_t transfers = 0;
    for (transfers = 0; transfers < min(downloads.size, MAX_PARALLEL); ++transfers) {
        add_download_to_multi_handle(downloads.elements[transfers], cm);
    }

    int still_alive = 1;
    do {
        curl_multi_perform(cm, &still_alive);

        CURLMsg *msg;
        int msgs_left = -1;
        while((msg = curl_multi_info_read(cm, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                FILE *file = NULL;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &file);
                fclose(file);

                char *url = NULL;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
                println(stderr,
                        "R: ", msg->data.result, " - ", curl_easy_strerror(msg->data.result),
                        " <", url, ">");

                CURL *e = msg->easy_handle;
                curl_multi_remove_handle(cm, e);
                curl_easy_cleanup(e);
            } else {
                println(stderr, "E: CURLMsg (", msg->msg, ")");
            }

            if (transfers < downloads.size) {
                add_download_to_multi_handle(downloads.elements[transfers++], cm);
            }
        }

        if(still_alive) curl_multi_wait(cm, NULL, 0, 1000, NULL);
    } while (still_alive || (transfers < downloads.size));

    // TODO: emote_downloader should download and setup the twitter emoji pack
    // Twitter Emoji Pack: https://twemoji.twitter.com/

    return 0;
}
