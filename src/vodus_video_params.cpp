#include "./vodus_video_params.hpp"

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
    print(stream, "}");
}
