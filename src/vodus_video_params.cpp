#include "./vodus_video_params.hpp"

void print1(FILE *stream, Video_Params params)
{
    println(stream, "{");
    println(stream, "    .fps = ", params.fps, ",");
    println(stream, "    .width = ", params.width, ",");
    println(stream, "    .height = ", params.height, ",");
    println(stream, "    .font_size = ", params.font_size, ",");
    println(stream, "    .background_colour = ", params.background_colour, ",");
    println(stream, "    .nickname_colour = ", params.nickname_colour, ",");
    println(stream, "    .text_colour = ", params.text_colour, ",");
    println(stream, "    .bitrate = ", params.bitrate, ",");
    print(stream, "}");
}
