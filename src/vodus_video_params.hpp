#ifndef VODUS_VIDEO_PARAMS_HPP_
#define VODUS_VIDEO_PARAMS_HPP_

struct Video_Params
{
    size_t fps;
    size_t width;
    size_t height;
    size_t font_size;
    Pixel32 background_color;
    Pixel32 nickname_color;
    Pixel32 text_color;
    int bitrate;
    String_View font;
};

void print1(FILE *stream, Video_Params params);

Video_Params default_video_params();
Video_Params video_params_from_file(const char *filepath);

#endif  // VODUS_VIDEO_PARAMS_HPP_
