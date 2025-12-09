#include "ScreenCaptureService.h"
#include "../encoder/VideoEncoder.h" 
#include "../encoder/RtcRtpSender.h" 
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QThread>

ScreenCaptureService::ScreenCaptureService(QObject* parent)
    : QObject(parent)
    , m_thread(nullptr)
    , m_worker(nullptr)
    , isBusy(false)
{
    init();
}

ScreenCaptureService::~ScreenCaptureService()
{
    stopCapture();
    // Qt parent-child tree frees memory; stop thread explicitly for safety

    m_thread->quit();
    m_thread->wait();
}

void ScreenCaptureService::init()
{
    m_thread = new QThread(this);
    m_worker = new VideoWorker();
    m_worker->moveToThread(m_thread);

    // use the started() signal so initResources runs in the worker thread event loop
    connect(m_thread, &QThread::started, m_worker, &VideoWorker::initResources, Qt::QueuedConnection);
    connect(m_thread, &QThread::finished, m_worker, &VideoWorker::cleanup);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &VideoWorker::frameProcessingFinished,
        this, &ScreenCaptureService::onWorkerFinished, Qt::QueuedConnection);

    m_thread->start();

    
    // 1. Create capture objects
    m_session = new QMediaCaptureSession(this);
    m_screenCapture = new QScreenCapture(this);
    m_previewWidget = new QVideoWidget(); // embed into UI when needed

    // 2. Wire pipeline
    m_session->setScreenCapture(m_screenCapture);
    m_session->setVideoOutput(m_previewWidget);

    // 3. Default to primary screen
    m_screenCapture->setScreen(QGuiApplication::primaryScreen());


    m_videoSink = new QVideoSink(this);
    m_session->setVideoSink(m_videoSink);
    // for test
    // m_session->setVideoOutput(m_videoSink); // would pop up a window

    // Connect to receive frames on each screen refresh
    connect(m_videoSink, &QVideoSink::videoFrameChanged, 
        this, &ScreenCaptureService::onFrameCaptured);
}

void ScreenCaptureService::startCapture()
{   
    //if (!m_encoder) {
    //    m_encoder = new VideoEncoder(this);
    //    // Example: 640x360 @15fps 4Mbps
    //    if (m_encoder->init(640, 360, 15, 4000000)) {
    //        INFO() << "Video Encoder Initialized!";
    //    }
    //    else {
    //        FATAL() << "Encoder Init Failed!";
    //        return;
    //    }

    //    // 3. Hook encoder output to signal
    //    m_encoder->onEncodedData = [this](const std::vector<uint8_t>& data, uint32_t ts) {
    //        QByteArray qData(reinterpret_cast<const char*>(data.data()), data.size());
    //        emit encodedFrameReady(qData, ts);
    //        // qDebug() << "Captured data is :" << data <<"\n";
    //        // stopCapture();
    //        };
    //}

    // If encoder not ready, warn and drop frames
    /*if (!m_encoder) {
        WARNING() << "Warning: Encoder not initialized yet. Frames will be dropped.";
    }*/

    if (m_screenCapture) {
        m_screenCapture->start();
        INFO() << "Screen Capture Started!";
        emit captureStateChanged(true);
    }
}

void ScreenCaptureService::stopCapture()
{
    if (m_screenCapture) {
        m_screenCapture->stop();
        INFO() << "Screen Capture Stopped!";
        emit captureStateChanged(false);
    }
}

QVideoWidget* ScreenCaptureService::getVideoPreviewWidget()
{
    return m_previewWidget;
}

VideoWorker* ScreenCaptureService::getWorker()
{
    return m_worker;
}

void ScreenCaptureService::onDCOpened(bool isCaller)
{
    if (isCaller) startCapture();
    // add render if needed
}

void ScreenCaptureService::onFrameCaptured()
{
    if (isBusy) return; // drop frame if worker busy to keep realtime

    // Grab current frame
    QVideoFrame frame = m_videoSink->videoFrame();
    if (!frame.isValid()) return;

    // Mark busy
    isBusy = true;

    // Queue to worker thread using queued connection; QVideoFrame is thread-safe when mapped once
    QMetaObject::invokeMethod(m_worker, "processFrame", Qt::QueuedConnection, Q_ARG(QVideoFrame, frame));
}

void ScreenCaptureService::onWorkerFinished() { isBusy = false; }

VideoWorker::VideoWorker(QObject* parent):
    QObject(parent),
    m_encoder(nullptr),
    m_rtpSender(nullptr)
{}

VideoWorker::~VideoWorker()
{}

void VideoWorker::initResources()
{
    m_encoder = new VideoEncoder(this);
    m_rtpSender = new RtcRtpSender(this);

    if (m_encoder->init(640, 360, 15, 4000000)) {
        INFO() << "Video Encoder Initialized!";
    }
    else {
        FATAL() << "Encoder Init Failed! Please check!";
        return;
    }

    // Wire encoder to RTP sender inside worker thread
    m_encoder->onEncodedData = ([this](const std::vector<uint8_t>& data, uint32_t timestamp) {
        if (m_rtpSender) {
            m_rtpSender->sendH264(data, timestamp);
        }
        });

    // Forward RTP packets to outer handler
    connect(m_rtpSender, &RtcRtpSender::rtpPacketReady,
        this, &VideoWorker::onPacketReady);

    INFO() << "Video Pipeline initialized on thread:" << QThread::currentThread();
}

void VideoWorker::processFrame(const QVideoFrame& frame)
{
    // Encode frame in worker thread
    if (m_encoder) m_encoder->encode(frame);

    // Notify controller so producer can send next frame
    emit frameProcessingFinished();
}

void VideoWorker::cleanup()
{
    m_encoder->deleteLater();
    m_rtpSender->deleteLater();
}

void VideoWorker::onPacketReady(const QByteArray& packetData)
{
    emit rtpPacketReady(packetData);
}

