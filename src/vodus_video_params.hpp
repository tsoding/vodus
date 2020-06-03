#ifndef VODUS_VIDEO_PARAMS_HPP_
#define VODUS_VIDEO_PARAMS_HPP_

struct Video_Params
{
    size_t fps;
    size_t width;
    size_t height;
    size_t font_size;
};

void print1(FILE *stream, Video_Params params);

#endif  // VODUS_VIDEO_PARAMS_HPP_
