#include "aids.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <limits.h>

using namespace aids;

void usage(FILE *stream)
{
    println(stream, "Usage: diffimg -e <expected.png> -a <actual.png> -t 25.0");
}

int main(int argc, char *argv[])
{
    Args args = {argc, argv};

    const char *expected_filepath = NULL;
    const char *actual_filepath = NULL;
    float threshold = 10.0f;

    args.shift();
    while (!args.empty()) {
        auto flag = cstr_as_string_view(args.shift());
        if (flag == "-e"_sv) {
            if (args.empty()) {
                println(stderr, "No argument for flag `", flag, "`");
                usage(stderr);
                exit(1);
            }
            expected_filepath = args.shift();
        } else if (flag == "-a"_sv) {
            if (args.empty()) {
                println(stderr, "No argument for flag `", flag, "`");
                usage(stderr);
                exit(1);
            }
            actual_filepath = args.shift();
        } else if (flag == "-t"_sv) {
            if (args.empty()) {
                println(stderr, "No argument for flag `", flag, "`");
                usage(stderr);
                exit(1);
            }

            const auto threshold_sv = cstr_as_string_view(args.shift());
            const auto maybe_threshold = threshold_sv.as_float();
            if (!maybe_threshold.has_value) {
                println(stderr, threshold_sv, " is not a float");
                usage(stderr);
                exit(1);
            }
            threshold = maybe_threshold.unwrap;
        } else {
            println(stderr, "Unknown flag `", flag, "`");
            usage(stderr);
            exit(1);
        }
    }

    if (expected_filepath == NULL) {
        println(stderr, "Expected path is not provied");
        usage(stderr);
        exit(1);
    }

    if (actual_filepath == NULL) {
        println(stderr, "Actual path is not provied");
        usage(stderr);
        exit(1);
    }

    int expected_width = 0;
    int expected_height = 0;
    uint32_t *expected_data = (uint32_t *) stbi_load(
        expected_filepath, &expected_width, &expected_height, NULL, 4);
    if (expected_data == NULL) {
        println(stderr, "Could not read file `", expected_filepath, "`");
        exit(1);
    }

    int actual_width = 0;
    int actual_height = 0;
    uint32_t *actual_data = (uint32_t *) stbi_load(
        actual_filepath, &actual_width, &actual_height, NULL, 4);
    if (actual_data == NULL) {
        println(stderr, "Could not read file `", actual_filepath, "`");
        exit(1);
    }

    if (actual_width != expected_width || actual_height != expected_height) {
        println(stderr, "Unexpected size!");
        println(stderr, "  Expected: ", expected_width, "x", expected_height);
        println(stderr, "  Actual:   ", actual_width, "x", actual_height);
        exit(1);
    }

    const auto all = expected_width * actual_height;
    int different = 0;
    for (int i = 0; i < all; ++i) {
        if (expected_data[i] != actual_data[i]) {
            different += 1;
        }
    }
    float deviation = ((float) different / (float) all) * 100.0f;
    println(stdout, "Deviation: ", deviation, "%");
    println(stdout, "Threshold: ", threshold, "%");

    if (deviation > threshold) {
        println(stdout, "TOO DIFFERENT");
        return 1;
    }

    println(stdout, "OK");
    return 0;
}
