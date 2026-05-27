#include "ScreenCaptureService.h"
#include "../encoder/VideoEncoder.h" 
#include "../encoder/RtcRtpSender.h" 
#include "../render/StreamReceiver.h"
#include "../render/VideoOpenGLWidget.h"
#include <cstring>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include <QThread>
#include <QImage>
#include <QPixmap>

ScreenCaptureService::ScreenCaptureService(QObject* parent)
    : QObject(parent)
    , m_session(nullptr)
    , m_screenCapture(nullptr)
    , m_videoSink(nullptr)
    , m_thread(nullptr)
    , m_worker(nullptr)
    , isBusy(false)
{}

ScreenCaptureService::~ScreenCaptureService()
{
    stopCapture();
    // Qt parent-child tree frees memory; stop thread explicitly for safety
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
    if (m_renderThread) {
        m_renderThread->quit();
        m_renderThread->wait();
    }
}

void ScreenCaptureService::init(bool isCaller)
{
    if (isCaller) { // Initialize the resources of the screen capture module
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

        // 2. Create single video sink (QMediaCaptureSession supports only ONE output)
        m_videoSink = new QVideoSink(this);
        
        // 3. Wire pipeline - use ONLY QVideoSink as output
        m_session->setScreenCapture(m_screenCapture);
        m_session->setVideoSink(m_videoSink);
        
        // 4. Default to primary screen
        m_screenCapture->setScreen(QGuiApplication::primaryScreen());
        
        // 5. Connect videoFrameChanged for encoding only (preview handled in UI)
        connect(m_videoSink, &QVideoSink::videoFrameChanged,
                this, &ScreenCaptureService::onFrameCaptured, Qt::DirectConnection);
        
        INFO() << "Capture pipeline initialized (sink for encoding, preview via UI)";
    }
    else { // Initialize the resources of the screen rendering module
        m_renderThread = new QThread(this);
        m_renderWorker = new RenderWorker();
        m_renderWorker->moveToThread(m_renderThread);

        m_renderWidget = new VideoOpenGLWidget();

        // Forward decoded frames to UI widget (queued, cross-thread safe)
        connect(m_renderWorker, &RenderWorker::frameReady,
            m_renderWidget, &VideoOpenGLWidget::onFrameDecoded,
            Qt::QueuedConnection);

        connect(m_renderThread, &QThread::finished, m_renderWorker, &QObject::deleteLater);
        m_renderThread->start();
    }
    emit resourceReady();
}

void ScreenCaptureService::startCapture()
{   
    if (m_screenCapture) {
        m_screenCapture->start();
        INFO() << "Screen Capture Started!";
        
        // Polling thread already started in init(); just mark capturing state
        m_isCapturing = true;
        emit captureStateChanged(true);
    }
}

void ScreenCaptureService::stopCapture()
{
    if (m_screenCapture) {
        m_isCapturing = false;
        m_screenCapture->stop();
        INFO() << "Screen Capture Stopped!";
        
        INFO() << "Capture stats - Total frames:" << m_frameCount
               << "Dropped (busy):" << m_dropCount;
        emit captureStateChanged(false);
    }
}

QVideoSink* ScreenCaptureService::getVideoSink()
{
    return m_videoSink;
}

VideoOpenGLWidget* ScreenCaptureService::getRenderWidget()
{
    return m_renderWidget;
}

VideoWorker* ScreenCaptureService::getVideoWorker()
{
    return m_worker;
}

RenderWorker* ScreenCaptureService::getRenderWorker()
{
    return m_renderWorker;
}

void ScreenCaptureService::onDCOpened(bool isCaller)
{
    init(isCaller);
    if (isCaller) startCapture();
    else startRender();
}

void ScreenCaptureService::startRender()
{
    if (m_renderThread && !m_renderThread->isRunning()) {
        m_renderThread->start();
    }
}

void ScreenCaptureService::onFrameCaptured()
{
    // Grab current frame from sink (polled, not signal-driven)
    QVideoFrame frame = m_videoSink->videoFrame();
    if (!frame.isValid()) return;

    ++m_frameCount;

    // Skip if worker is busy (encoder still processing previous frame)
    if (isBusy) {
        ++m_dropCount;
        return;
    }

    // Mark busy and queue for encoding
    isBusy = true;

    // Queue to worker thread using queued connection
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

RenderWorker::RenderWorker(QObject* parent)
    : QObject(parent)
    , m_receiver(new StreamReceiver(this))
{
    connect(m_receiver, &StreamReceiver::frameReady,
        this, &RenderWorker::frameReady,
        Qt::QueuedConnection);
    m_receiver->ensureDecoder(AV_CODEC_ID_H264);
}

RenderWorker::~RenderWorker() = default;

void RenderWorker::onEncodedPacket(const QByteArray& packetData)
{
    if (!m_receiver) return;
    auto flushFrame = [this](bool force){
        if (!m_hasFrame) return;
        // 如果帧内没有 SPS/PPS 而有缓存且是 IDR，则补充
        bool hasSPSInFrame = false, hasPPSInFrame = false;
        for (auto& n : m_pendingNals) {
            if (n.size() > 4) {
                uint8_t t = n[4] & 0x1F;
                if (t == 7) hasSPSInFrame = true;
                if (t == 8) hasPPSInFrame = true;
            }
        }
        if (m_pendingHasIDR && (!hasSPSInFrame || !hasPPSInFrame) && !m_sps.empty() && !m_pps.empty()) {
            std::vector<uint8_t> spsNAL; spsNAL.reserve(m_sps.size()+4); spsNAL.insert(spsNAL.end(),{0,0,0,1}); spsNAL.insert(spsNAL.end(), m_sps.begin(), m_sps.end());
            std::vector<uint8_t> ppsNAL; ppsNAL.reserve(m_pps.size()+4); ppsNAL.insert(ppsNAL.end(),{0,0,0,1}); ppsNAL.insert(ppsNAL.end(), m_pps.begin(), m_pps.end());
            m_pendingNals.insert(m_pendingNals.begin(), std::move(ppsNAL));
            m_pendingNals.insert(m_pendingNals.begin(), std::move(spsNAL));
        }

        size_t total = 0; for (auto& n: m_pendingNals) total += n.size();
        if (total == 0) { m_pendingNals.clear(); m_hasFrame=false; m_pendingHasIDR=false; return; }
        std::vector<std::byte> buffer(total);
        size_t off=0; for (auto& n: m_pendingNals){ memcpy(buffer.data()+off, n.data(), n.size()); off+=n.size(); }
        // DEBUG() << "Flush frame ts:" << m_currentTimestamp << "NAL count:" << m_pendingNals.size() << "total:" << total;
        m_receiver->onTrackData(buffer);
        m_pendingNals.clear();
        m_pendingHasIDR=false;
        m_hasFrame=false;
    };

    const uint8_t* data = reinterpret_cast<const uint8_t*>(packetData.constData());
    int size = packetData.size();
    if (size < 12) return;
    if ((data[0] & 0xC0) != 0x80) return; // not RTP

    bool marker = data[1] & 0x80;
    uint32_t ts = (uint32_t(data[4]) << 24) | (uint32_t(data[5]) << 16) | (uint32_t(data[6]) << 8) | uint32_t(data[7]);
    uint16_t seq = (uint16_t(data[2]) << 8) | data[3];

    // 帧切换：新 timestamp 前先冲刷上一帧
    if (m_hasFrame && ts != m_currentTimestamp) {
        flushFrame(true);
        m_fuState.active = false;
    }
    if (!m_hasFrame) {
        m_currentTimestamp = ts;
        m_hasFrame = true;
    }

    const uint8_t* payload = data + 12;
    int payloadSize = size - 12;
    if (payloadSize <= 0) return;

    uint8_t nalType = payload[0] & 0x1F;

    // DEBUG() << "TS:" << ts << "seq:" << seq << "NAL type:" << int(nalType) << "payload:" << payloadSize;
    std::vector<uint8_t> annexb;

    if (nalType >= 1 && nalType <= 23) {
        annexb.reserve(payloadSize + 4);
        annexb.insert(annexb.end(), {0,0,0,1});
        annexb.insert(annexb.end(), payload, payload + payloadSize);

        if (nalType == 7) { m_sps.assign(payload, payload + payloadSize); }// DEBUG() << "Cached SPS size:" << m_sps.size(); }
        else if (nalType == 8) { m_pps.assign(payload, payload + payloadSize); }// DEBUG() << "Cached PPS size:" << m_pps.size(); }
        else if (nalType == 5) { m_pendingHasIDR = true; }
    }
    else if (nalType == 28 && payloadSize >= 2) { // FU-A
        uint8_t fuIndicator = payload[0];
        uint8_t fuHeader = payload[1];
        bool S = fuHeader & 0x80;
        bool E = fuHeader & 0x40;
        uint8_t origNal = fuHeader & 0x1F;
        uint8_t reconstructedHdr = (fuIndicator & 0xE0) | origNal;

        if (S) {
            m_fuState.buffer.clear();
            m_fuState.buffer.insert(m_fuState.buffer.end(), {0,0,0,1});
            m_fuState.buffer.push_back(reconstructedHdr);
            m_fuState.buffer.insert(m_fuState.buffer.end(), payload + 2, payload + payloadSize);
            m_fuState.active = true;
            m_fuState.expectedSeq = seq + 1;
            m_fuState.nalType = origNal;
        } else if (m_fuState.active) {
            if (seq != m_fuState.expectedSeq) { m_fuState.active = false; return; }
            m_fuState.buffer.insert(m_fuState.buffer.end(), payload + 2, payload + payloadSize);
            m_fuState.expectedSeq = seq + 1;
        }

        if (m_fuState.active && E) {
            annexb = m_fuState.buffer;
            if (m_fuState.nalType == 5) m_pendingHasIDR = true;
            m_fuState.active = false;
        }
    }

    if (!annexb.empty()) {
        m_pendingNals.push_back(std::move(annexb));
    }

    // Do not flush on marker alone; rely on timestamp change to gather full frame
}

