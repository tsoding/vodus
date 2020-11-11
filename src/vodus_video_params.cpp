#include "./vodus_video_params.hpp"

const char *string_view_as_cstr(String_View sv)
{
    char *cstr = (char *) malloc(sv.count + 1);
    if (!cstr) return cstr;
    memcpy(cstr, sv.data, sv.count);
    cstr[sv.count] = '\0';
    return cstr;
}

void print1(FILE *stream, Output_Type output_type)
{
    switch (output_type) {
    case Output_Type::Video:
        print1(stream, "video");
        break;
    case Output_Type::PNG:
        print1(stream, "png");
        break;
    case Output_Type::Preview:
        print1(stream, "preview");
        break;
    }
}

void print1(FILE *stream, Video_Params params)
{
    println(stream, "{");
    println(stream, "    .output_type = ", params.output_type);
    println(stream, "    .fps = ", params.fps);
    println(stream, "    .width = ", params.width);
    println(stream, "    .height = ", params.height);
    println(stream, "    .font_size = ", params.font_size);
    println(stream, "    .background_color = ", params.background_color);
    println(stream, "    .nickname_color = ", params.nickname_color);
    println(stream, "    .text_color = ", params.text_color);
    println(stream, "    .bitrate = ", params.bitrate);
    println(stream, "    .font = ", params.font);
    println(stream, "    .message_limit = ", params.messages_limit);
    print(stream, "}");
}

Video_Params default_video_params() {
    Video_Params params = {};
    params.output_type       = Output_Type::Video;
    params.fps               = 60;
    params.width             = 1920;
    params.height            = 1080;
    params.font_size         = 128;
    params.background_color  = {32, 32, 32, 255};
    params.nickname_color    = {255, 100, 100, 255};
    params.text_color        = {200, 200, 200, 255};
    params.bitrate           = 400'000;
    params.font              = ""_sv;
    params.messages_limit    = {};
    return params;
}

void usage(FILE *stream)
{
    println(stream, "Usage: vodus [OPTIONS] <log-filepath>");
    println(stream, "    --help|-h                       Display this help and exit");
    println(stream, "    --output|-o <filepath>          Output path");
    println(stream, "    --font <filepath>               Path to the Font face file");
    println(stream, "    --font-size <number>            The size of the Font face");
    println(stream, "    --limit <number>                Limit the amount of messages to render");
    println(stream, "    --width <number>                Width of the output video");
    println(stream, "    --height <number>               Height of the output video");
    println(stream, "    --fps <number>                  Frames per second of the output video");
    println(stream, "    --background-color <color-hex>  Color of the background");
    println(stream, "    --nickname-color <color-hex>    Color of the nickname part of the rendered message");
    println(stream, "    --text-color <color-hex>        Color of the text part of the rendered message");
    println(stream, "    --bitrate <number>              The average bitrate in bits/s (probably, libavcodec\n"
                    "                                    didn't really tell me the exact units)");
    println(stream, "  Values:");
    println(stream, "    <filepath>                      Path to a file according to the platform format");
    println(stream, "    <number>                        Positive integer");
    println(stream, "    <color-hex>                     8 hex characters digits describing RGBA.\n"
                    "                                    Case insensitive. No leading # character.\n"
                    "                                    Examples: ffffffff, 000000FF, A0eA0000");
}

template <typename Integer>
Integer parse_integer_flag(String_View flag, String_View value)
{
    auto integer = value.as_integer<size_t>();
    if (!integer.has_value) {
        println(stderr, "`", flag, "` expected a number, but `", value, "` is not a number! D:");
        abort();
    }
    return integer.unwrap;
}

Pixel32 parse_color_flag(String_View flag, String_View value)
{
    auto color = hexstr_as_pixel32(value);
    if (!color.has_value) {
        println(stderr, "`", flag, "` expected a color, but `", value, "` is not a color! D:");
        abort();
    }
    return color.unwrap;
}

void patch_video_params_from_flag(Video_Params *params, String_View flag, String_View value);

void patch_video_params_from_file(Video_Params *params, String_View filepath)
{
    auto filepath_cstr = string_view_as_cstr(filepath);
    defer(free((void*) filepath_cstr));

    auto content = read_file_as_string_view(filepath_cstr);
    if (!content.has_value) {
        println(stderr, "Could not read `", filepath, "`");
        abort();
    }

    while (content.unwrap.count > 0) {
        auto line = content.unwrap.chop_by_delim('\n').trim();
        auto flag = line.chop_by_delim('=').trim();
        auto value = line.trim();

        patch_video_params_from_flag(params, flag, value);
    }
}


size_t levenshtein(String_View a, String_View b)
{
    size_t *dp = new size_t[(a.count + 1) * (b.count + 1)];
#define DP(row, column) dp[(row) * (b.count + 1) + (column)]
    defer(delete[] dp);
    memset(dp, 0, sizeof(size_t) * (a.count + 1) * (b.count + 1));

    for (size_t i = 0; i <= a.count; ++i) {
        for (size_t j = 0; j <= b.count; ++j) {
            if (min(i, j) == 0) {
                DP(i, j) = max(i, j);
            } else {
                DP(i, j) = min(
                    DP(i - 1, j) + 1,
                    DP(i, j - 1) + 1,
                    DP(i - 1, j - 1) + (a.data[i - 1] != b.data[j - 1]));
            }
        }
    }

    return DP(a.count, b.count);
#undef DP
}

void patch_video_params_from_flag(Video_Params *params, String_View flag, String_View value)
{
    if (flag == "fps"_sv) {
        params->fps = parse_integer_flag<size_t>(flag, value);
    } else if (flag == "width"_sv) {
        params->width = parse_integer_flag<size_t>(flag, value);
    } else if (flag == "height"_sv) {
        params->height = parse_integer_flag<size_t>(flag, value);
    } else if (flag == "font_size"_sv || flag == "font-size"_sv) {
        params->font_size = parse_integer_flag<size_t>(flag, value);
    } else if (flag == "background_color"_sv || flag == "background-color"_sv) {
        params->background_color = parse_color_flag(flag, value);
    } else if (flag == "nickname_color"_sv || flag == "nickname-color"_sv) {
        params->nickname_color = parse_color_flag(flag, value);
    } else if (flag == "text_color"_sv || flag == "text-color"_sv) {
        params->text_color = parse_color_flag(flag, value);
    } else if (flag == "bitrate"_sv) {
        params->bitrate = parse_integer_flag<int>(flag, value);
    } else if (flag == "font"_sv) {
        params->font = value;
    } else if (flag == "messages_limit"_sv || flag == "messages-limit"_sv) {
        params->messages_limit = {true, parse_integer_flag<size_t>(flag, value)};
    } else if (flag == "output_type"_sv || flag == "output-type"_sv) {
        if (value == "video"_sv) {
            params->output_type = Output_Type::Video;
        } else if (value == "png"_sv) {
            params->output_type = Output_Type::PNG;
        } else if (value == "preview"_sv) {
            params->output_type = Output_Type::Preview;
        } else {
            println(stderr, "Unknown output type `", value, "`");
            abort();
        }
    } else {
        size_t n = ULONG_MAX;
        String_View corrected_flag = {};
        for (size_t i = 0; i < param_names_count; ++i) {
            auto m = levenshtein(flag, param_names[i]);
            if (m < n) {
                n = m;
                corrected_flag = param_names[i];
            }
        }

        const size_t LEVENSHTEIN_CORRECTION_THRESHOLD = 5;
        if (n <= LEVENSHTEIN_CORRECTION_THRESHOLD) {
            println(stderr, "Unknown flag `", flag, "`. Maybe you meant `", corrected_flag, "`");
        } else {
            println(stderr, "Unknown flag `", flag, "`.");
        }
        abort();
    }
}

void patch_video_params_from_args(Video_Params *params, Args *args)
{
    while (!args->empty()) {
        auto flag = cstr_as_string_view(args->shift());
        flag.chop(2);

        if (args->empty()) {
            println(stderr, "[ERROR] no parameter for flag `", flag, "` found");
            usage(stderr);
            abort();
        }
        auto value = cstr_as_string_view(args->shift());

        if (flag == "config"_sv) {
            patch_video_params_from_file(params, value);
        } else {
            patch_video_params_from_flag(params, flag, value);
        }
    }
}
