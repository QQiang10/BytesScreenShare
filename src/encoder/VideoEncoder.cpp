#include "VideoEncoder.h"
#include <QDebug>
#include <libavutil/frame.h>

VideoEncoder::VideoEncoder(QObject* parent) : QObject(parent) {
    m_pkt = av_packet_alloc();
}

VideoEncoder::~VideoEncoder() {
    cleanup();
}

bool VideoEncoder::init(int width, int height, int fps, int bitrate) {
    m_targetW = width;
    m_targetH = height;
    m_fps = fps > 0 ? fps : 30;

    // 1. Find H.264 encoder
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        FATAL() << "H.264 Encoder not found!";
        return false;
    }

    // 2. Allocate codec context
    m_codecCtx = avcodec_alloc_context3(codec);
    m_codecCtx->bit_rate = bitrate;
    m_codecCtx->width = width;
    m_codecCtx->height = height;
    m_codecCtx->time_base = { 1, fps };
    m_codecCtx->framerate = { fps, 1 };
    m_codecCtx->gop_size = 10; // keyframe interval
    m_codecCtx->max_b_frames = 0; // realtime: no B-frames
    m_codecCtx->pix_fmt = AV_PIX_FMT_YUV420P; // standard format

    // 3. Open encoder (ultrafast + zerolatency)
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "ultrafast", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);

    // m_codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_codecCtx, codec, &opts) < 0) {
        qDebug() << "Could not open codec";
        return false;
    }

    // 在 VideoEncoder::init() 中，打开编码器后立即添加：
    if (m_codecCtx->extradata && m_codecCtx->extradata_size > 0) {
        DEBUG() << "Encoder extradata size:" << m_codecCtx->extradata_size;
        // 这里面就有 SPS+PPS，可以在第一帧时主动发送
    }

    // 4. Allocate YUV frame buffers
    m_frameYUV = av_frame_alloc();
    m_frameYUV->format = m_codecCtx->pix_fmt;
    m_frameYUV->width = m_codecCtx->width;
    m_frameYUV->height = m_codecCtx->height;
    av_frame_get_buffer(m_frameYUV, 32);

    return true;
}

void VideoEncoder::encode(const QVideoFrame& inputFrame) {
    if (!m_codecCtx) return;

    // A. Map Qt frame memory
    QVideoFrame cloneFrame = inputFrame;
    if (!cloneFrame.map(QVideoFrame::ReadOnly)) {
        FATAL() << "Map frame failed";
        return;
    }

    // B. Resolution/format conversion (SWS scale)
    // Downscale higher resolutions to 1920x1080 YUV420P
    if (!m_swsCtx || cloneFrame.width() != m_lastSrcW ||
        cloneFrame.height() != m_lastSrcH) {
        qDebug() << "Source resolution changed to" << cloneFrame.width() << "x" << cloneFrame.height() << "- Recreating SwsContext";

        // Free previous context if existed
        if (m_swsCtx) {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }

        // Record new source resolution
        m_lastSrcW = cloneFrame.width();
        m_lastSrcH = cloneFrame.height();

        
        // Create scaling context: source BGRA to target YUV420P
        m_swsCtx = sws_getContext(
            cloneFrame.width(), cloneFrame.height(), AV_PIX_FMT_BGRA, // ����
            m_targetW, m_targetH, AV_PIX_FMT_YUV420P,               // ���
            SWS_BICUBIC, nullptr, nullptr, nullptr
        );
    }

    // Bail out if SWS context creation failed
    if (!m_swsCtx) {
        cloneFrame.unmap();
        return;
    }

    // Perform scaling
    const uint8_t* srcData[4] = { cloneFrame.bits(0) };
    int srcLinesize[4] = { cloneFrame.bytesPerLine(0) };

    sws_scale(m_swsCtx, srcData, srcLinesize, 0, cloneFrame.height(),
        m_frameYUV->data, m_frameYUV->linesize);

    cloneFrame.unmap();

    // C. Send frame to encoder
    m_frameYUV->pts = m_frameCount++; // simple PTS counter
    int ret = avcodec_send_frame(m_codecCtx, m_frameYUV);

    // D. Receive encoded packets
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecCtx, m_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        else if (ret < 0) break;

        if (onEncodedData) {
            uint8_t* data = m_pkt->data;
            int size = m_pkt->size;

            // Parse Annex-B NAL units separated by 00 00 01 or 00 00 00 01
            int curPos = 0;
            while (curPos < size) {
                // Find start code
                int nalStart = -1;
                int prefixLen = 0;

                // Simple start code detection; FFmpeg outputs Annex-B with start codes

                // Directly check current position for start code
                if (curPos + 4 <= size && data[curPos] == 0 && data[curPos + 1] == 0 && data[curPos + 2] == 0 && data[curPos + 3] == 1) {
                    nalStart = curPos + 4;
                    prefixLen = 4;
                }
                else if (curPos + 3 <= size && data[curPos] == 0 && data[curPos + 1] == 0 && data[curPos + 2] == 1) {
                    nalStart = curPos + 3;
                    prefixLen = 3;
                }

                if (nalStart == -1) {
                    // No start code found; break to avoid infinite loop
                    break;
                }

                // Find next start code to get current NAL size
                int nextNalStart = size; // default to end
                for (int i = nalStart; i < size - 3; ++i) {
                    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
                        nextNalStart = i; // 00 00 01
                        // If preceding byte is 0, treat as 00 00 00 01
                        if (i > 0 && data[i - 1] == 0) nextNalStart = i - 1;
                        break;
                    }
                }

                int nalSize = nextNalStart - nalStart;
                if (nalSize > 0) {
                    std::vector<uint8_t> nalBuffer(data + nalStart, data + nalStart + nalSize);

                    // Convert PTS to 90 kHz RTP timestamp
                    
                    uint32_t rtpTimestamp = 0;
                    if (m_pkt->pts != AV_NOPTS_VALUE) {
                        int denom = (m_fps > 0) ? m_fps : 30;
                        rtpTimestamp = static_cast<uint32_t>(m_pkt->pts * (90000 / denom));
                    }

                    // Callback with NAL data and timestamp
                    onEncodedData(nalBuffer, rtpTimestamp);
                }

                int nalType = data[nalStart] & 0x1F;
                DEBUG() << "NAL type:" << nalType << "size:" << nalSize;
                if (nalType == 7) DEBUG() << "  -> SPS";
                if (nalType == 8) DEBUG() << "  -> PPS";
                if (nalType == 5) DEBUG() << "  -> IDR";
                curPos = nextNalStart;
            }
        }
        av_packet_unref(m_pkt);
    }
}

void VideoEncoder::cleanup() {
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    // Release buffers correctly
    if (m_frameYUV) {
        av_frame_free(&m_frameYUV);
        m_frameYUV = nullptr;
    }
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_pkt) {
        av_packet_free(&m_pkt);
        m_pkt = nullptr;
    }
}