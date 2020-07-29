#include "./vodus_video_params.hpp"

const char *string_view_as_cstr(String_View sv)
{
    char *cstr = (char *) malloc(sv.count + 1);
    if (!cstr) return cstr;
    memcpy(cstr, sv.data, sv.count);
    cstr[sv.count] = '\0';
    return cstr;
}

void print1(FILE *stream, Video_Params params)
{
    println(stream, "{");
    println(stream, "    .fps = ", params.fps, ",");
    println(stream, "    .width = ", params.width, ",");
    println(stream, "    .height = ", params.height, ",");
    println(stream, "    .font_size = ", params.font_size, ",");
    println(stream, "    .background_color = ", params.background_color, ",");
    println(stream, "    .nickname_color = ", params.nickname_color, ",");
    println(stream, "    .text_color = ", params.text_color, ",");
    println(stream, "    .bitrate = ", params.bitrate, ",");
    println(stream, "    .font = ", params.font, ",");
    println(stream, "    .message_limit = ", params.messages_limit, ",");
    print(stream, "}");
}

Video_Params default_video_params() {
    Video_Params params = {};
    params.fps               = 60;
    params.width             = 1920;
    params.height            = 1080;
    params.font_size         = 128;
    params.background_color  = {32, 32, 32, 255};
    params.nickname_color    = {255, 100, 100, 255};
    params.text_color        = {200, 200, 200, 255};
    params.bitrate           = 400'000;
    params.font              = ""_sv;
    params.messages_limit     = VODUS_MESSAGES_CAPACITY;
    return params;
}

#define expect_into(lvalue, maybe, message, ...)    \
    do {                                            \
        auto maybe_var = (maybe);                   \
        if (!maybe_var.has_value) {                 \
            println(stderr, message, __VA_ARGS__);  \
            abort();                                \
        }                                           \
        (lvalue) = maybe_var.unwrap;                \
    } while (0)


Video_Params video_params_from_file(const char *filepath)
{
    Video_Params result = default_video_params();
    String_View content = {};
    expect_into(
        content, read_file_as_string_view(filepath),
        "Could not read file `", filepath, "`");

    for (int line_number = 1; content.count > 0; ++line_number) {
        auto line = content.chop_by_delim('\n').trim();
        if (line.count != 0) {
            auto key = line.chop_by_delim('=').trim();
            auto value = line.trim();

            if ("fps"_sv == key) {
                expect_into(
                    result.fps, value.as_integer<size_t>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else if ("width"_sv == key) {
                expect_into(
                    result.width, value.as_integer<size_t>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else if ("height"_sv == key) {
                expect_into(
                    result.height, value.as_integer<size_t>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else if ("font_size"_sv == key) {
                expect_into(
                    result.font_size, value.as_integer<size_t>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else if ("background_color"_sv == key) {
                expect_into(
                    result.background_color, hexstr_as_pixel32(value),
                    filepath, ":", line_number, ": `", value, "` is not a color");
            } else if ("nickname_color"_sv == key) {
                expect_into(
                    result.nickname_color, hexstr_as_pixel32(value),
                    filepath, ":", line_number, ": `", value, "` is not a color");
            } else if ("text_color"_sv == key) {
                expect_into(
                    result.text_color, hexstr_as_pixel32(value),
                    filepath, ":", line_number, ": `", value, "` is not a color");
            } else if ("bitrate"_sv == key) {
                expect_into(
                    result.bitrate, value.as_integer<int>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else if ("font"_sv == key) {
                result.font = value;
            } else if ("limit"_sv == key) {
                expect_into(
                    result.messages_limit, value.as_integer<size_t>(),
                    filepath, ":", line_number, ": `", value, "` is not a number");
            } else {
                println(stderr, "Unknown Video Parameter `", key, "`");
                abort();
            }
        }
    }

    return result;
}
