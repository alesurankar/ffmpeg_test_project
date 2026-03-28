extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

#include <string>


class WebcamCapture
{
public:
    WebcamCapture(const std::string& deviceName, const std::string& outputDir);
    ~WebcamCapture();
    bool Init();
    bool CaptureFrame();
    bool ConvertToRGB();
    bool SetupPNGEncoder();
    bool EncodeAndSave();
private:
    void Cleanup();
private:
    std::string device_name_;
    std::string output_dir_;
    AVFormatContext* fmt_ctx_;
    AVCodecContext* codec_ctx_;
    AVCodecContext* png_ctx_;
    AVPacket* packet_;
    AVFrame* frame_;
    AVFrame* rgb_frame_;
    SwsContext* sws_ctx_;
    AVPacket* png_packet_ = nullptr;
    uint8_t* buffer_ = nullptr;
    int video_stream_index_ = 0;
    AVDictionary* options_ = nullptr;
};