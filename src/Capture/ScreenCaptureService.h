#pragma once

#include <QObject>
#include <QVideoSink>
#include <QMediaCaptureSession>
#include <QScreenCapture>
#include <QVideoWidget>
#include <QByteArray>
#include <QByteArrayView>
#include <memory>  // for std::unique_ptr

#include "signaling-server/src/Common.hpp"
class VideoEncoder;
class RtcRtpSender;


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

    // UI embed handle
    QVideoWidget* getVideoPreviewWidget();

    VideoWorker* getWorker();

public:
    void onDCOpened(bool isCaller);
    void onFrameCaptured();
    void onWorkerFinished();
signals:
    // Optional: notify capture state changes
    void captureStateChanged(bool isRunning);

    void videoDataReady(const std::vector<uint8_t>& data);
    void encodedFrameReady(const QByteArray& encodedData, uint32_t timestamp);
    void packetReady(const QByteArray& packet);

private:
    void init();

    QMediaCaptureSession* m_session = nullptr;
    QScreenCapture* m_screenCapture = nullptr;
    QVideoWidget* m_previewWidget = nullptr; // preview window
    QVideoSink* m_videoSink = nullptr; // sink to read frames

    QThread* m_thread;
    VideoWorker* m_worker;

    bool isBusy;

    // WebRTC RTP sender placeholder
    // std::unique_ptr<RtcRtpSender> m_rtcSender;
};