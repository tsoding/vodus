#include "vodus_encoder.hpp"

void encode_avframe(AVCodecContext *context, AVFrame *frame, AVPacket *pkt, FILE *outfile)
{
    avec(avcodec_send_frame(context, frame));

    for (int ret = 0; ret >= 0; ) {
        ret = avcodec_receive_packet(context, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return;
        } else {
            avec(ret);
        }

        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
}

void slap_image32_onto_avframe(Image32 frame_image32, AVFrame *avframe)
{
    assert(avframe->width == (int) frame_image32.width);
    assert(avframe->height == (int) frame_image32.height);

    for (int y = 0; y < avframe->height; ++y) {
        for (int x = 0; x < avframe->width; ++x) {
            Pixel32 p = frame_image32.pixels[y * frame_image32.width + x];
            int Y =  (0.257 * p.r) + (0.504 * p.g) + (0.098 * p.b) + 16;
            int V =  (0.439 * p.r) - (0.368 * p.g) - (0.071 * p.b) + 128;
            int U = -(0.148 * p.r) - (0.291 * p.g) + (0.439 * p.b) + 128;
            avframe->data[0][y        * avframe->linesize[0] + x]        = Y;
            avframe->data[1][(y >> 1) * avframe->linesize[1] + (x >> 1)] = U;
            avframe->data[2][(y >> 1) * avframe->linesize[2] + (x >> 1)] = V;
        }
    }
}

void avencoder_encode(AVEncoder_Context *context, Image32 surface, int frame_index)
{
    slap_image32_onto_avframe(surface, context->frame);
    context->frame->pts = frame_index;
    encode_avframe(context->context, context->frame, context->packet, context->output_stream);
}

void free_avencoder_context(AVEncoder_Context *context)
{
    avcodec_free_context(&context->context);
    av_packet_free(&context->packet);
    fclose(context->output_stream);
    av_frame_free(&context->frame);
    delete context;
}

AVEncoder_Context *new_avencoder_context(Video_Params params, const char *output_filepath)
{
    AVEncoder_Context *result = new AVEncoder_Context();

    result->codec = fail_if_null(
        avcodec_find_encoder(AV_CODEC_ID_MPEG2VIDEO),
        "Codec not found");

    result->context = fail_if_null(
        avcodec_alloc_context3(result->codec),
        "Could not allocate video codec context");

    result->context->bit_rate = params.bitrate;
    result->context->width = params.width;
    result->context->height = params.height;
    result->context->time_base = (AVRational){1, (int) params.fps};
    result->context->framerate = (AVRational){(int) params.fps, 1};
    result->context->gop_size = 10;
    // result->context->max_b_frames = 1;
    result->context->pix_fmt = AV_PIX_FMT_YUV420P;

    result->packet = fail_if_null(
        av_packet_alloc(),
        "Could not allocate packet");


    avec(avcodec_open2(result->context, result->codec, NULL));

    if (output_filepath == nullptr) {
        println(stderr, "Error: Output filepath is not provided. Use `--output` flag.");
        usage(stderr);
        exit(1);
    }

    result->output_stream = fail_if_null(
        fopen(output_filepath, "wb"),
        "Could not open ", output_filepath);


    result->frame = fail_if_null(
        av_frame_alloc(),
        "Could not allocate video frame");

    result->frame->format = result->context->pix_fmt;
    result->frame->width  = result->context->width;
    result->frame->height = result->context->height;

    avec(av_frame_get_buffer(result->frame, 32));

    return result;
}

void pngencoder_encode(PNGEncoder_Context *context, Image32 surface, int frame_index)
{
    char buffer[1024];
    String_Buffer path = {sizeof(buffer), buffer};
    sprint(&path, context->output_folder_path, "/", frame_index, "-frame.png");
    if (!stbi_write_png(buffer, surface.width, surface.height, 4, surface.pixels, surface.width * 4)) {
        println(stderr, "Could not save image: ", path);
        abort();
    }
}

const char *vertex_shader_source =
    "#version 130\n"
    "out vec2 texcoord;"
    "void main(void)\n"
    "{\n"
    "    int gray = gl_VertexID ^ (gl_VertexID >> 1);\n"
    "    gl_Position = vec4(\n"
    "        2 * (gray / 2) - 1,\n"
    "        2 * (gray % 2) - 1,\n"
    "        0.0,\n"
    "        1.0);\n"
    "    texcoord = vec2(gray / 2, 1 - gray % 2);\n"
    "}\n";
const char *fragment_shader_source =
    "#version 130\n"
    "in vec2 texcoord;\n"
    "uniform sampler2D frame;\n"
    "out vec4 color;\n"
    "void main(void) {\n"
    "    color = texture(frame, texcoord);\n"
    "    //color = vec4(0.5, 0.5, 0.5, 1.0);\n"
    "}\n";

struct Shader
{
    GLuint unwrap;
};

Shader compile_shader(const char *source_code, GLenum shader_type)
{
    GLuint shader = {};
    shader = glCreateShader(shader_type);
    if (shader == 0) {
        println(stderr, "Could not create a shader");
        exit(1);
    }

    glShaderSource(shader, 1, &source_code, 0);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLchar buffer[1024];
        int length = 0;
        glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
        println(stderr, "Could not compile shader: ", buffer);
        exit(1);
    }

    return {shader};
}

struct Program
{
    GLuint unwrap;
};

Program link_program(Shader vertex_shader, Shader fragment_shader)
{
    GLuint program = glCreateProgram();

    if (program == 0) {
        println(stderr, "Could not create shader program");
        exit(1);
    }

    glAttachShader(program, vertex_shader.unwrap);
    glAttachShader(program, fragment_shader.unwrap);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLchar buffer[1024];
        int length = 0;
        glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
        println(stdout, "Could not link the program: ", buffer);
        exit(1);
    }

    return {program};
}

Preview_Context *new_preview_context(Video_Params params)
{
    Preview_Context *context = new Preview_Context();

    if (!glfwInit()) {
        println(stderr, "Could not initialize GLFW");
        exit(1);
    }

    context->window = glfwCreateWindow(params.width, params.height, "Vodus", NULL, NULL);
    if (!context->window) {
        println(stderr, "Could not create GLFW window");
        exit(1);
    }

    glfwMakeContextCurrent(context->window);

    // glfwSetFramebufferSizeCallback(window, framebuffer_resize);

    GLuint texture_id;

    glGenTextures(1, &texture_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    println(stdout, "Compiling vertex shader...");
    Shader vertex_shader = compile_shader(vertex_shader_source, GL_VERTEX_SHADER);
    println(stdout, "Compiling fragment shader...");
    Shader fragment_shader = compile_shader(fragment_shader_source, GL_FRAGMENT_SHADER);
    println(stdout, "Linking the program...");
    Program program = link_program(vertex_shader, fragment_shader);

    glUseProgram(program.unwrap);

    GLint frame_sampler = glGetUniformLocation(program.unwrap, "frame");
    glUniform1i(frame_sampler, 0);

    return context;
}

void previewencoder_encode(Preview_Context *context, Image32 surface, int frame_index)
{
    if (glfwWindowShouldClose(context->window)) {
        // TODO(#140): encoder does not provide a mechanism to exit prematurely
        exit(0);
    }

    // Rebind the texture
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 surface.width,
                 surface.height,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 surface.pixels);
    glGenerateMipmap(GL_TEXTURE_2D);

    // TODO: Preview is not synced with real time
    // TODO: Preview does not allow to pause and move backward/forward

    glDrawArrays(GL_QUADS, 0, 4);
    glfwSwapBuffers(context->window);
    glfwPollEvents();
}
