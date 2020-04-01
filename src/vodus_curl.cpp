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

    void clean()
    {
        memset(this->data, 0, Capacity + 1);
        this->size = 0;
    }
};

String_Buffer buffer = {};

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
