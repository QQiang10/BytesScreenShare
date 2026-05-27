#pragma once

#include <QObject>
#include <QtGlobal>
#include <QByteArray>
#include <QtEndian>
#include <memory>
#include <vector>
#include <rtc/rtc.hpp>

class RtcRtpSender : public QObject
{
    Q_OBJECT
public:
    explicit RtcRtpSender(QObject* parent = nullptr);
    ~RtcRtpSender();

    // Send an H.264 NAL unit; payload will be packetized into RTP FU-A as needed
    // nalData: raw NAL without the start code (00 00 00 01)
    // timestamp: 90 kHz clock timestamp
    void sendH264(const std::vector<uint8_t>& nalData, uint32_t timestamp);

signals:
    //void onLocalSdpReady(const QString& sdp);
    //void onIceCandidate(const QString& candidate, const QString& mid);
    //void onDataChannelOpen(); //  ����һ��Ҫд�꣡
    //void onDataChannelClosed();
    void rtpPacketReady(const QByteArray& packet);

private:
    /*void ensurePeerConnection();
    void ensureDataChannel();*/

    // Low-level RTP packet send helper
    void sendRtpPacket(const std::vector<uint8_t>& payload, bool marker);

private:
    uint16_t sequenceNumber_ = 0;
    uint32_t ssrc_ = 0;
    uint32_t currentTimestamp_ = 0;

    const int payloadType_ = 96;
    const size_t MAX_RTP_PAYLOAD_SIZE = 1100; // payload size chosen to account for headers
};