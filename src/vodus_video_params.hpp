#ifndef VODUS_VIDEO_PARAMS_HPP_
#define VODUS_VIDEO_PARAMS_HPP_

enum class Output_Type
{
    Video,
    PNG
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
};

void print1(FILE *stream, Video_Params params);

Video_Params default_video_params();

#endif  // VODUS_VIDEO_PARAMS_HPP_
