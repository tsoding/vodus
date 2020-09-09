#ifndef VODUS_ENCODER_HPP_
#define VODUS_ENCODER_HPP_

typedef void (*Encode_Func)(void *context, Image32 surface, int frame_index);

struct Encoder
{
    void *context;
    Encode_Func encode_func;

    void encode(Image32 surface, int frame_index)
    {
        encode_func(context, surface, frame_index);
    }
};

struct AVEncoder_Context
{
    AVFrame *frame;
    AVCodecContext *context;
    AVPacket *packet;
    FILE *output_stream;
};

void avencoder_encode(AVEncoder_Context *context, Image32 surface, int frame_index);

struct PNGEncoder_Context
{
    String_View output_folder_path;
};

void pngencoder_encode(PNGEncoder_Context *context, Image32 surface, int frame_index);

#endif  // VODUS_ENCODER_HPP_
