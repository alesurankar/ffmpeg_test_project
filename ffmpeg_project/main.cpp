extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <fstream>

int main()
{
    avdevice_register_all();
    avformat_network_init();

    AVFormatContext* fmt_ctx = nullptr;

    const char* device_name = "video=USB2.0 HD UVC WebCam";

    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "30", 0);

    const AVInputFormat* input_format = av_find_input_format("dshow");

    if (avformat_open_input(&fmt_ctx, device_name, input_format, &options) != 0) {
        std::cout << "Failed to open webcam!\n";
        return -1;
    }

    std::cout << "Webcam opened successfully!\n";

    // Get stream info
    avformat_find_stream_info(fmt_ctx, nullptr);

    // Find video stream
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cout << "No video stream found!\n";
        return -1;
    }

    // Setup decoder
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    std::cout << "Capturing frame...\n";

    // Read until we get one frame
    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_stream_index) {
            if (avcodec_send_packet(codec_ctx, packet) == 0) {
                if (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    std::cout << "Frame captured!\n";
                    break;
                }
            }
        }
        av_packet_unref(packet);
    }

    // Convert to RGB
    SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    AVFrame* rgb_frame = av_frame_alloc();
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(num_bytes);

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, buffer,
        AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, 1);

    sws_scale(sws_ctx, frame->data, frame->linesize, 0,
        codec_ctx->height, rgb_frame->data, rgb_frame->linesize);

    // Save as PPM image
    std::ofstream file("output.ppm", std::ios::binary);
    file << "P6\n" << codec_ctx->width << " " << codec_ctx->height << "\n255\n";

    for (int y = 0; y < codec_ctx->height; y++) {
        file.write((char*)rgb_frame->data[0] + y * rgb_frame->linesize[0],
            codec_ctx->width * 3);
    }

    file.close();

    std::cout << "Saved image as output.ppm\n";


    // Cleanup
    av_free(buffer);
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    av_dict_free(&options);
    avformat_network_deinit();

    return 0;
}