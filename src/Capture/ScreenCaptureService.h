#pragma once

#include <QObject>
#include <QVideoSink>
#include <QMediaCaptureSession>
#include <QScreenCapture>
#include <QByteArray>
#include <QByteArrayView>
#include <QTimer>
#include <memory>  // for std::unique_ptr

#include "signaling-server/src/Common.hpp"
class VideoEncoder;
class RtcRtpSender;
class StreamReceiver;
class VideoOpenGLWidget;


class VideoWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoWorker(QObject* parent = nullptr);
    ~VideoWorker();

public slots:
    // Process one video frame (queued)
    void processFrame(const QVideoFrame& frame);

public:
    // Initialize resources (runs in worker thread)
    // Creates encoder and RTP sender within the worker thread
    void initResources();

    void cleanup();

    void onPacketReady(const QByteArray& packetData);

signals:
    // Signal: frame finished so the producer thread can send next one
    void frameProcessingFinished();

    // Signal: RTP packet ready, delivered via queued connection to consumer
    void rtpPacketReady(const QByteArray& packetData);

private:
    // Owned pointers (worker thread)
    VideoEncoder* m_encoder;
    RtcRtpSender* m_rtpSender;
};

// Render worker: runs in worker thread and feeds StreamReceiver
class RenderWorker : public QObject {
    Q_OBJECT
public:
    explicit RenderWorker(QObject* parent = nullptr);
    ~RenderWorker();

public slots:
    void onEncodedPacket(const QByteArray& packetData);

signals:
    void frameReady(const uchar* y,const uchar* u,const uchar* v,int yStride,int uStride,int vStride,int width,int height);

private:
    StreamReceiver* m_receiver;
    std::vector<uint8_t> m_sps;  // Cache SPS
    std::vector<uint8_t> m_pps;  // Cache PPS
    // 帧级组装状态
    uint32_t m_currentTimestamp = 0;
    bool m_hasFrame = false;
    bool m_pendingHasIDR = false;
    std::vector<std::vector<uint8_t>> m_pendingNals;
    // FU-A 重组状态
    struct FuState {
        std::vector<uint8_t> buffer;
        uint16_t expectedSeq = 0;
        bool active = false;
        uint8_t nalType = 0;
    } m_fuState;
};

// QObject subclass to use signals/slots
class ScreenCaptureService : public QObject
{
    Q_OBJECT // Qt ��

public:
    explicit ScreenCaptureService(QObject* parent = nullptr);
    ~ScreenCaptureService();

    // UI-facing controls
    void startCapture();
    void stopCapture();

    // Access to video sink for external preview rendering
    QVideoSink* getVideoSink();
    VideoOpenGLWidget* getRenderWidget();

    VideoWorker* getVideoWorker();
    RenderWorker* getRenderWorker();

public:
    void onDCOpened(bool isCaller);
    void onFrameCaptured();
    void onWorkerFinished();
    void startRender();
signals:
    // Optional: notify capture state changes
    void captureStateChanged(bool isRunning);

    void videoDataReady(const std::vector<uint8_t>& data);
    void encodedFrameReady(const QByteArray& encodedData, uint32_t timestamp);
    void packetReady(const QByteArray& packet);
    void resourceReady();

private:
    void init(bool isCaller);

private:
    // For Capture
    QMediaCaptureSession* m_session;
    QScreenCapture* m_screenCapture;
    QVideoSink* m_videoSink; // single sink for both preview and encoding

    QThread* m_thread;
    VideoWorker* m_worker;
    bool isBusy;
    bool m_isCapturing = false;
    quint64 m_frameCount = 0;
    quint64 m_dropCount = 0;

    // For Render
    QThread* m_renderThread = nullptr;
    RenderWorker* m_renderWorker = nullptr;
    VideoOpenGLWidget* m_renderWidget = nullptr;
    

    // WebRTC RTP sender placeholder
    // std::unique_ptr<RtcRtpSender> m_rtcSender;
};