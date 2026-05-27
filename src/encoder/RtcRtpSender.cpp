#include "RtcRtpSender.h"
#include <QDebug>
#include <random>
#include <cstring> // for memcpy

RtcRtpSender::RtcRtpSender(QObject* parent)
    : QObject(parent)
{
    // Randomize initial SSRC
    std::random_device rd;
    ssrc_ = rd();
}

RtcRtpSender::~RtcRtpSender()
{}


// -------------------------------------------------------------------------
//  RFC 6184 H.264 RTP packetization
// -------------------------------------------------------------------------
void RtcRtpSender::sendH264(const std::vector<uint8_t>& nalData, uint32_t timestamp)
{

    currentTimestamp_ = timestamp; // update timestamp for this frame

    // H.264 NALU Header (1 byte)
    // [F|NRI|Type]
    uint8_t nalHeader = nalData[0];
    uint8_t nalType = nalHeader & 0x1F;

    // Case 1: Single NAL unit packet (fits in one RTP packet)
    if (nalData.size() <= MAX_RTP_PAYLOAD_SIZE) {
        RtcRtpSender::sendRtpPacket(nalData, true); // Marker = 1 (һ֡����)
        return;
    }

    // Case 2: Fragmentation units (FU-A) for oversized NAL units
    // RFC 6184 Section 5.8

    // Skip the original NAL header; only payload is fragmented
    const uint8_t* payloadData = nalData.data() + 1;
    size_t payloadSize = nalData.size() - 1;
    size_t offset = 0;

    while (offset < payloadSize) {
        // Compute current fragment size
        size_t chunkSize = std::min(MAX_RTP_PAYLOAD_SIZE - 2, payloadSize - offset);
        // -2 because FU-A needs 2 bytes for indicator and header

        bool isFirstPacket = (offset == 0);
        bool isLastPacket = (offset + chunkSize == payloadSize);

        std::vector<uint8_t> fuaPacket;
        fuaPacket.reserve(chunkSize + 2);

        // 1. FU Indicator: [F|NRI|Type], Type=28 (FU-A)
        uint8_t fuIndicator = (nalHeader & 0xE0) | 28;
        fuaPacket.push_back(fuIndicator);

        // 2. FU Header: [S|E|R|Type]; S=start, E=end, R=0
        uint8_t fuHeader = nalType;
        if (isFirstPacket) fuHeader |= 0x80; // Set S bit
        if (isLastPacket)  fuHeader |= 0x40; // Set E bit
        fuaPacket.push_back(fuHeader);

        // 3. Payload
        fuaPacket.insert(fuaPacket.end(), payloadData + offset, payloadData + offset + chunkSize);

        // Send fragment; marker only on last fragment
        RtcRtpSender::sendRtpPacket(fuaPacket, isLastPacket);

        offset += chunkSize;
    }
}

void RtcRtpSender::sendRtpPacket(const std::vector<uint8_t>& payload, bool marker)
{
    // RTP header is 12 bytes
    //std::vector<uint8_t> packet;
    //packet.resize(12 + payload.size());

    //uint8_t* header = reinterpret_cast<uint8_t*>(packet.data());

    //// Byte 0: V=2, P=0, X=0, CC=0 -> 0x80
    //header[0] = 0x80;

    //// Byte 1: M (Marker), PT (Payload Type)
    //header[1] = (marker ? 0x80 : 0x00) | (payloadType_ & 0x7F);

    //// Byte 2-3: Sequence Number (Big Endian)
    //header[2] = (sequenceNumber_ >> 8) & 0xFF;
    //header[3] = sequenceNumber_ & 0xFF;
    //sequenceNumber_++;

    //// Byte 4-7: Timestamp (Big Endian)
    //header[4] = (currentTimestamp_ >> 24) & 0xFF;
    //header[5] = (currentTimestamp_ >> 16) & 0xFF;
    //header[6] = (currentTimestamp_ >> 8) & 0xFF;
    //header[7] = currentTimestamp_ & 0xFF;

    //// Byte 8-11: SSRC (Big Endian)
    //header[8] = (ssrc_ >> 24) & 0xFF;
    //header[9] = (ssrc_ >> 16) & 0xFF;
    //header[10] = (ssrc_ >> 8) & 0xFF;
    //header[11] = ssrc_ & 0xFF;

    //// ���� Payload
    //std::memcpy(reinterpret_cast<uint8_t*>(packet.data()) + 12, payload.data(), payload.size());

    //emit rtpPacketReady(packet);

    // using qt original type QByteArray
    QByteArray packet;
    packet.resize(12 + payload.size());

    uchar* header = reinterpret_cast<uchar*>(packet.data());

    // Byte 0: V=2, P=0, X=0, CC=0 -> 0x80
    header[0] = 0x80;

    // Byte 1: M (Marker), PT (Payload Type)
    header[1] = (marker ? 0x80 : 0x00) | (payloadType_ & 0x7F);

    // Byte 2-3: Sequence Number (Big Endian)
    qToBigEndian<quint16>(sequenceNumber_, header + 2);
    sequenceNumber_++;

    // Byte 4-7: Timestamp (Big Endian)
    qToBigEndian<quint32>(currentTimestamp_, header + 4);

    // Byte 8-11: SSRC (Big Endian)
    qToBigEndian<quint32>(ssrc_, header + 8);

    // Copy payload
    memcpy(packet.data() + 12, payload.data(), payload.size());

    emit rtpPacketReady(packet);
}