#include <cstdio>
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pthread.h>
#define CURL_STRICTER
#include <curl/curl.h>
#include "./tzozen.h"
#include "./core.cpp"

const size_t DEFAULT_STRING_BUFFER_CAPACITY = 20 * 1024;

template <size_t Capacity = DEFAULT_STRING_BUFFER_CAPACITY>
struct String_Buffer
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

    void clean()
    {
        memset(this->data, 0, Capacity + 1);
        this->size = 0;
    }
};

template <size_t Capacity>
void print1(FILE *stream, String_Buffer<Capacity> buffer)
{
    fwrite(buffer.data, 1, buffer.size, stream);
}

size_t curl_string_buffer_write_callback(char *ptr,
                                         size_t size,
                                         size_t nmemb,
                                         String_Buffer<> *string_buffer)
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
                                       String_Buffer<Capacity> *string_buffer)
{
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_string_buffer_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, string_buffer);
    return curl_easy_perform(curl);
}

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
const long MAX_PARALLEL = 100;

struct Curl_Multi_Downloader
{
    CURLM *cm;
    size_t transfers;

    void finish_transfer(CURL *ch)
    {
        char *url = NULL;
        curl_easy_getinfo(ch, CURLINFO_EFFECTIVE_URL, &url);
        fprintf(stdout, "%s is DONE\n", url);

        FILE *file = NULL;
        curl_easy_getinfo(ch, CURLINFO_PRIVATE, &file);
        fclose(file);

        curl_multi_remove_handle(cm, ch);
        curl_easy_cleanup(ch);

        transfers -= 1;
    }

    void add_transfer(CURL *ch)
    {
        int still_alive = 1;
        while (still_alive || (transfers >= MAX_PARALLEL)) {
            curl_multi_perform(cm, &still_alive);
            CURLMsg *msg;
            int msgs_left = -1;
            while((msg = curl_multi_info_read(cm, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    finish_transfer(msg->easy_handle);
                } else {
                    fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
                }

                if (still_alive) curl_multi_wait(cm, NULL, 0, 1000, NULL);
            }
        }

        transfers += 1;
        curl_multi_add_handle(cm, ch);
    }

    void join()
    {
        int still_alive = 1;
        do {
            curl_multi_perform(cm, &still_alive);
            CURLMsg *msg;
            int msgs_left = -1;
            while((msg = curl_multi_info_read(cm, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    finish_transfer(msg->easy_handle);
                } else {
                    fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
                }

                if (still_alive) curl_multi_wait(cm, NULL, 0, 1000, NULL);
            }
        } while (still_alive || (transfers >= MAX_PARALLEL));
    }
};

void append_bttv_mapping(CURL *curl, const char *emotes_url, Curl_Multi_Downloader *cmd)
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

        FILE *file = fopen(path_buffer.data, "wb");
        if (file == NULL) {
            println(stderr, "Could not open `", path_buffer, "`: ", strerror(errno));
            abort();
        }

        CURL *ch = curl_easy_init();
        curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, fwrite);
        curl_easy_setopt(ch, CURLOPT_URL, url_buffer.data);
        curl_easy_setopt(ch, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(ch, CURLOPT_PRIVATE, file);
        cmd->add_transfer(ch);
    });
}

int create_directory_if_not_exists(const char *dirpath)
{
    struct stat statbuf = {};
    int res = stat(dirpath, &statbuf);

    if (res == -1) {
        if (errno == ENOENT) {
            // TODO: create_directory_if_not_exists does not create parent folders
            return mkdir(dirpath, 0755);
        } else {
            return -1;
        }
    }

    if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
        errno = ENOTDIR;
        return -1;
    }

    return 0;
}

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

    Curl_Multi_Downloader cmd = {};
    cmd.cm = curl_multi_init();

    curl_multi_setopt(cmd.cm, CURLMOPT_MAXCONNECTS, MAX_PARALLEL);

    append_bttv_mapping(curl, "https://api.betterttv.net/2/emotes", &cmd);
    append_bttv_mapping(curl, "https://api.betterttv.net/2/channels/tsoding", &cmd);
    cmd.join();

    return 0;
}
