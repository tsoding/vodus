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
    params.font              = "";
    params.messages_limit     = VODUS_MESSAGES_CAPACITY;
    return params;
}
