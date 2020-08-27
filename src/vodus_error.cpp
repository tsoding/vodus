void avec(int code)
{
    if (code < 0) {
        const size_t buf_size = 256;
        char buf[buf_size];
        av_strerror(code, buf, buf_size);
        println(stderr, "ffmpeg pooped itself: ", buf);
        exit(1);
    }
}

template <typename T, typename... Args>
T *fail_if_null(T *ptr, Args... args)
{
    if (ptr == nullptr) {
        println(stderr, args...);
        exit(1);
    }

    return ptr;
}
