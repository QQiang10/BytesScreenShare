#pragma once
#include <QObject>
#include <QVideoFrame>
#include <functional>
#include <signaling-server/src/Common.hpp>

// FFmpeg in C headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoEncoder : public QObject
{
    Q_OBJECT
public:
    explicit VideoEncoder(QObject* parent = nullptr);
    ~VideoEncoder();

    // Initialize encoder (default target 1080p; ScreenCapture can override)
    bool init(int width, int height, int fps, int bitrate);

    // Encode one Qt frame
    void encode(const QVideoFrame& frame);

    // Callback to deliver encoded H.264 payload
    std::function<void(const std::vector<uint8_t>&, uint32_t)> onEncodedData;

private:
    // Cleanup resources
    void cleanup();

    AVCodecContext* m_codecCtx = nullptr;
    AVFrame* m_frameYUV = nullptr;     // converted YUV frame buffer
    SwsContext* m_swsCtx = nullptr;    // scaling and format conversion context
    AVPacket* m_pkt = nullptr;

    int m_targetW = 1920; // target resolution defaults to 1080p
    int m_targetH = 1080;
    int m_frameCount = 0;
    int m_fps = 30;

    int m_lastSrcW = -1; // track last source resolution to detect changes
    int m_lastSrcH = -1;
};