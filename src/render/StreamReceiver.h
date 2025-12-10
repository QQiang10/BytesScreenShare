#pragma once
#ifndef STREAMRECEIVER_H
#define STREAMRECEIVER_H

#include <QObject>
#include<QByteArray>
#include<QMutex>
#include<memory>
#include<vector>
#include<string>

#include "signaling-server/src/Common.hpp"
#include<rtc/rtc.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include<libavutil/frame.h>
#include<libavutil/imgutils.h>
#include<libswscale/swscale.h>
}

// StreamReceiver 主要功能：
//   1. 作为接收端处理 WebRTC 视频。
//   2. 接收远端发送的视频数据。
//   3. 使用 FFmpeg 解码视频数据。
//   4. 解码后将 YUV 数据通过信号发送给 UI。
class StreamReceiver : public QObject {
	Q_OBJECT

public:

	explicit StreamReceiver(QObject* parent = nullptr);
	~StreamReceiver();
		// 启动接收流程，signalingUrl: 信令服务器地址，myId: 本端ID，peerId: 发送端ID（对方）
	void start(const std::string& signalingUrl,const std::string& myId,const std::string& peerId);
		// 停止接收并断开连接
	void stop();
	// 确保解码器已初始化（非信令模式使用）
	bool ensureDecoder(AVCodecID id = AV_CODEC_ID_H264);
	// 处理 WebRTC 收到的数据
	void onTrackData(const std::vector<std::byte>& data);

signals:
		// 视频帧解码成功信号，数据为 YUV420P，y,u,v: 数据指针，yStride,uStride,vStride: 每行字节数，width,height: 视频尺寸
	void frameReady(const uchar* y,const uchar* u,const uchar* v,int yStride,int uStride,int vStride,int width,int height);

		// 通知 UI 状态变化，如 "Connecting"、"Connected"、"Disconnected"、"Error"
	void stateChange(const QString& state);

private:
		// WebRTC 相关对象
	std::shared_ptr<rtc::PeerConnection> m_pc;
	std::shared_ptr<rtc::WebSocket> m_ws;

		// FFmpeg 解码相关对象
	AVCodecContext* m_codecCtx=nullptr;//��������������
	AVFrame* m_frame = nullptr;//������ԭʼ֡
	AVPacket* m_packet = nullptr;//���յ��ı����
	const AVCodec* m_codec = nullptr;//ָ���������
		// 解码器解析器，用于处理 H264 分片/合并，组装完整 NALU

		// 内部状态
	bool m_isDecoderInited = false;
	std::string m_myId;
	std::string m_peerId;

		// 初始化 FFmpeg 解码器
	bool initDecoder(AVCodecID id);
		// 释放 FFmpeg 解码器
	void CleanupDecoder();

		// 建立信令连接
	void setupSignaling(const std::string& url);
};

#endif //STREAMRECEIVER_H