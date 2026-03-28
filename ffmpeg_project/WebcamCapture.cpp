#include "WebcamCapture.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/imgutils.h>
}

#include <iostream>
#include <direct.h>

WebcamCapture::WebcamCapture(const std::string& deviceName, const std::string& outputDir)
    :
    device_name_(deviceName), output_dir_(outputDir),
    fmt_ctx_(nullptr), codec_ctx_(nullptr), png_ctx_(nullptr),
    packet_(nullptr), frame_(nullptr), rgb_frame_(nullptr), sws_ctx_(nullptr)
{}

WebcamCapture::~WebcamCapture()
{
    Cleanup();
}

bool WebcamCapture::Init()
{
    // Initialize FFmpeg
    avdevice_register_all();
    avformat_network_init();

    // Setup options
    av_dict_set(&options_, "video_size", "1280x720", 0);
    av_dict_set(&options_, "framerate", "30", 0);

    const AVInputFormat* input_format = av_find_input_format("dshow");
    if (!input_format) {
        std::cerr << "dshow input format not found!\n";
        return false;
    }

    if (avformat_open_input(&fmt_ctx_, device_name_.c_str(), input_format, &options_) != 0) {
        std::cerr << "Failed to open webcam!\n";
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        std::cerr << "Failed to get stream info!\n";
        return false;
    }

    // Find video stream
    video_stream_index_ = -1;
    for (unsigned int i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            break;
        }
    }

    if (video_stream_index_ == -1) {
        std::cerr << "No video stream found!\n";
        return false;
    }

    // Setup decoder
    AVCodecParameters* codecpar = fmt_ctx_->streams[video_stream_index_]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        std::cerr << "Decoder not found!\n";
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx_, codecpar);
    if (avcodec_open2(codec_ctx_, decoder, nullptr) < 0) {
        std::cerr << "Failed to open decoder!\n";
        return false;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();

    return true;
}

bool WebcamCapture::CaptureFrame()
{
    std::cout << "Capturing frame...\n";
    while (av_read_frame(fmt_ctx_, packet_) >= 0) {
        if (packet_->stream_index == video_stream_index_) {
            if (avcodec_send_packet(codec_ctx_, packet_) == 0) {
                if (avcodec_receive_frame(codec_ctx_, frame_) == 0) {
                    std::cout << "Frame captured!\n";
                    return true;
                }
            }
        }
        av_packet_unref(packet_);
    }
    return false;
}

bool WebcamCapture::ConvertToRGB()
{
    sws_ctx_ = sws_getContext(
        codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
        codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    if (!sws_ctx_) return false;

    rgb_frame_ = av_frame_alloc();
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx_->width, codec_ctx_->height, 1);
    buffer_ = (uint8_t*)av_malloc(num_bytes);
    av_image_fill_arrays(rgb_frame_->data, rgb_frame_->linesize, buffer_,
        AV_PIX_FMT_RGB24, codec_ctx_->width, codec_ctx_->height, 1);

    sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0,
        codec_ctx_->height, rgb_frame_->data, rgb_frame_->linesize);

    rgb_frame_->width = codec_ctx_->width;
    rgb_frame_->height = codec_ctx_->height;
    rgb_frame_->format = AV_PIX_FMT_RGB24;

    return true;
}

bool WebcamCapture::SetupPNGEncoder()
{
    const AVCodec* png_codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
    if (!png_codec) {
        std::cerr << "PNG encoder not found!\n";
        return false;
    }

    png_ctx_ = avcodec_alloc_context3(png_codec);
    png_ctx_->width = codec_ctx_->width;
    png_ctx_->height = codec_ctx_->height;
    png_ctx_->pix_fmt = AV_PIX_FMT_RGB24;
    png_ctx_->time_base = { 1, 25 };

    if (avcodec_open2(png_ctx_, png_codec, nullptr) < 0) {
        std::cerr << "Failed to open PNG codec!\n";
        return false;
    }

    png_packet_ = av_packet_alloc();
    return true;
}

bool WebcamCapture::EncodeAndSave()
{
    if (avcodec_send_frame(png_ctx_, rgb_frame_) < 0) {
        std::cerr << "Error sending frame to PNG encoder\n";
        return false;
    }

    while (true) {
        int ret = avcodec_receive_packet(png_ctx_, png_packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) {
            std::cerr << "Error encoding PNG\n";
            return false;
        }

        std::string save_path = output_dir_ + "\\output.png";
        FILE* f = nullptr;
        if (fopen_s(&f, save_path.c_str(), "wb") == 0 && f) {
            fwrite(png_packet_->data, 1, png_packet_->size, f);
            fclose(f);
            std::cout << "Saved PNG successfully at: " << save_path << std::endl;
        }
        else {
            std::cerr << "Failed to open " << save_path << std::endl;
            return false;
        }

        av_packet_unref(png_packet_);
    }

    return true;
}

void WebcamCapture::Cleanup()
{
    if (png_packet_) av_packet_free(&png_packet_);
    if (png_ctx_) avcodec_free_context(&png_ctx_);
    if (rgb_frame_) av_frame_free(&rgb_frame_);
    if (buffer_) av_free(buffer_);
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (frame_) av_frame_free(&frame_);
    if (packet_) av_packet_free(&packet_);
    if (codec_ctx_) avcodec_free_context(&codec_ctx_);
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);
    av_dict_free(&options_);
    avformat_network_deinit();
}