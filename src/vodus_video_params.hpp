#ifndef VODUS_VIDEO_PARAMS_HPP_
#define VODUS_VIDEO_PARAMS_HPP_

enum class Output_Type
{
    Video,
    PNG,
    Preview
};

struct Video_Params
{
    Output_Type output_type;
    size_t fps;
    size_t width;
    size_t height;
    size_t font_size;
    Pixel32 background_color;
    Pixel32 nickname_color;
    Pixel32 text_color;
    int bitrate;
    String_View font;
    Maybe<size_t> messages_limit;
    String_View message_regex;
};

String_View param_names[] = {
    "output_type"_sv,
    "fps"_sv,
    "width"_sv,
    "height"_sv,
    "font_size"_sv,
    "background_color"_sv,
    "nickname_color"_sv,
    "text_color"_sv,
    "bitrate"_sv,
    "font"_sv,
    "messages_limit"_sv,
    "message_regex"_sv,
};
const size_t param_names_count = sizeof(param_names) / sizeof(param_names[0]);

void print1(FILE *stream, Video_Params params);

Video_Params default_video_params();

#endif  // VODUS_VIDEO_PARAMS_HPP_
