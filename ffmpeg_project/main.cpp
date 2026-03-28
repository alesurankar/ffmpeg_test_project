extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <direct.h>

int main()
{
    // Print current working directory
    char* raw_cwd = _getcwd(nullptr, 0);  // allocate C-string
    if (!raw_cwd) {
        std::cerr << "Failed to get current working directory!" << std::endl;
        return -1;
    }
    std::string cwd(raw_cwd);
    std::cout << "Current working directory: " << cwd << std::endl;
    free(raw_cwd);

    // Initialize FFmpeg
    avdevice_register_all();
    avformat_network_init();

    // Webcam device
    const char* device_name = "video=USB2.0 HD UVC WebCam";
    const AVInputFormat* input_format = av_find_input_format("dshow");
    if (!input_format) {
        std::cerr << "dshow input format not found!" << std::endl;
        return -1;
    }

    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", "1280x720", 0);
    av_dict_set(&options, "framerate", "30", 0);

    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, device_name, input_format, &options) != 0) {
        std::cerr << "Failed to open webcam!\n";
        return -1;
    }
    std::cout << "Webcam opened successfully!\n";

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "Failed to get stream info!\n";
        return -1;
    }

    // Find the video stream
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        std::cerr << "No video stream found!\n";
        return -1;
    }

    // Setup decoder
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Decoder not found!\n";
        return -1;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, decoder, nullptr) < 0) {
        std::cerr << "Failed to open decoder!\n";
        return -1;
    }

    // Allocate packet and frame
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    std::cout << "Capturing frame...\n";
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

    // Convert to RGB24
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

    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
        rgb_frame->data, rgb_frame->linesize);

    // Setup PNG encoder
    const AVCodec* png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec) {
        std::cerr << "PNG encoder not found!\n";
        return -1;
    }

    AVCodecContext* png_ctx = avcodec_alloc_context3(png_codec);
    png_ctx->width = codec_ctx->width;
    png_ctx->height = codec_ctx->height;
    png_ctx->pix_fmt = AV_PIX_FMT_RGB24;
    png_ctx->time_base = { 1, 25 };

    if (avcodec_open2(png_ctx, png_codec, nullptr) < 0) {
        std::cerr << "Failed to open PNG codec!\n";
        return -1;
    }

    // Prepare frame for PNG encoder
    rgb_frame->width = codec_ctx->width;
    rgb_frame->height = codec_ctx->height;
    rgb_frame->format = AV_PIX_FMT_RGB24; 

    // Encode frame
    AVPacket* png_packet = av_packet_alloc();
    if (avcodec_send_frame(png_ctx, rgb_frame) < 0) {
        std::cerr << "Error sending frame to PNG encoder\n";
    }
    else {
        while (true) {
            int ret = avcodec_receive_packet(png_ctx, png_packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) {
                std::cerr << "Error encoding PNG\n";
                break;
            }

            std::string save_path = cwd + "\\output.png";
            std::cout << "Full save path: " << save_path << std::endl;

            FILE* f = nullptr;
            if (fopen_s(&f, save_path.c_str(), "wb") == 0 && f) {
                fwrite(png_packet->data, 1, png_packet->size, f);
                fclose(f);
                std::cout << "Saved PNG successfully at: " << save_path << std::endl;
            }
            else {
                std::cerr << "Failed to open " << save_path << std::endl;
            }

            av_packet_unref(png_packet);
        }
    }

    // Cleanup
    av_packet_free(&png_packet);
    avcodec_free_context(&png_ctx);
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