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
