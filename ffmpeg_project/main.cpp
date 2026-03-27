extern "C" {
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
}

#include <iostream>

int main() {
    avdevice_register_all();
    avformat_network_init();

    AVFormatContext* fmt_ctx = nullptr;

    const char* device_name = "video=USB2.0 HD UVC WebCam";

    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", "640x480", 0);
    av_dict_set(&options, "framerate", "30", 0);

    const AVInputFormat* input_format = av_find_input_format("dshow");

    if (!input_format) {
        std::cout << "DirectShow input format not found!" << std::endl;
        return -1;
    }

    if (avformat_open_input(&fmt_ctx, device_name, input_format, &options) != 0) {
        std::cout << "Failed to open webcam!" << std::endl;
        return -1;
    }

    std::cout << "Webcam opened successfully!" << std::endl;

    avformat_close_input(&fmt_ctx);
    av_dict_free(&options);
    avformat_network_deinit();

    return 0;
}