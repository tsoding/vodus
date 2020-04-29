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

template <typename T>
T min(T a, T b)
{
    return a < b ? a : b;
}

template <typename T, size_t Capacity>
struct Fixed_Array
{
    size_t size;
    T elements[Capacity];

    void push(T x)
    {
        elements[size++] = x;
    }
};

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
const size_t MAX_PARALLEL = 20;

struct Curl_Download
{
    String_Buffer<PATH_BUFFER_SIZE> url;
    String_Buffer<PATH_BUFFER_SIZE> file;

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

void append_bttv_mapping(CURL *curl,
                         const char *emotes_url,
                         FILE *mapping,
                         Fixed_Array<Curl_Download, 1024> *bttv_urls)
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

        Curl_Download download = {};

        download.file.write("emotes/"); // TODO: check for download.path.write return 09
        download.file.write(emote_id.string.data, emote_id.string.len);
        download.file.write(".");
        download.file.write(emote_imageType.string.data, emote_imageType.string.len);

        download.url.write("https://cdn.betterttv.net/emote/");
        download.url.write(emote_id.string.data, emote_id.string.len);
        download.url.write("/3x");

        println(mapping, emote_code.string, ",", download.file);
        bttv_urls->push(download);
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

Fixed_Array<Curl_Download, 1024> downloads;

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

    CURLM *cm = curl_multi_init();
    defer(curl_multi_cleanup(cm));

    curl_multi_setopt(cm, CURLMOPT_MAXCONNECTS, MAX_PARALLEL);

    append_bttv_mapping(curl, "https://api.betterttv.net/2/emotes", mapping, &downloads);
    append_bttv_mapping(curl, "https://api.betterttv.net/2/channels/tsoding", mapping, &downloads);

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
                fprintf(stderr, "R: %d - %s <%s>\n",
                        msg->data.result, curl_easy_strerror(msg->data.result), url);

                CURL *e = msg->easy_handle;
                curl_multi_remove_handle(cm, e);
                curl_easy_cleanup(e);
            } else {
                fprintf(stderr, "E: CURLMsg (%d)\n", msg->msg);
            }

            if (transfers < downloads.size) {
                add_download_to_multi_handle(downloads.elements[transfers++], cm);
            }

            if(still_alive) curl_multi_wait(cm, NULL, 0, 1000, NULL);
        }
    } while (still_alive || (transfers < downloads.size));

    return 0;
}
