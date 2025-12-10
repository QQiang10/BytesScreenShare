# include "StreamReceiver.h"
#include<QDebug>
#include<QJsonDocument>
#include<QJsonObject>

// FFMPEG 检查宏：输出错误信息
#define FFMPEG_CHECK(ret,msg)\
	if (ret < 0) {\
		char errbuf[AV_ERROR_MAX_STRING_SIZE];\
		av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);\
		qCritical() << msg << ":" << errbuf;\
		return;\
	}

StreamReceiver::StreamReceiver(QObject * parent):QObject(parent) {
	// 预分配 AVPacket 和 AVFrame
	m_packet = av_packet_alloc();
	m_frame = av_frame_alloc();
}

StreamReceiver::~StreamReceiver() {
	stop();
	if (m_packet) av_packet_free(&m_packet);
    if (m_frame) av_frame_free(&m_frame);
}

void StreamReceiver::start(const std::string& signalUrl,const std::string& myId,const std::string& peerId) {
	this->m_myId = myId;
	this->m_peerId=peerId;

	// 1. 初始化 WebRTC
	rtc::Configuration config;
	// 使用 Google 公共 STUN 服务器，穿透 NAT
	config.iceServers.emplace_back("stun:stun.l.google.com:19302");
	// 保存创建的 WebRTC 对象
	m_pc = std::make_shared<rtc::PeerConnection>(config);

	// 2. 绑定 Track 事件
	m_pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
		if (track->description().type() == "video") {
			qDebug() << "Received Video Track!";
			// 初始化解码器
			if (!initDecoder(AV_CODEC_ID_H264)) {
				emit stateChange("error");
				return;
			}
			// 收到第一帧后绑定消息回调
			track->onMessage([this](rtc::message_variant data) {
				if (std::holds_alternative<rtc::binary>(data)) {
					const auto& bin = std::get<rtc::binary>(data);
					onTrackData(bin);
				}
			});
		}

		});
	// 3. 绑定 ICE Candidate 回调（收集连接候选地址信息）
	m_pc->onLocalCandidate([this](rtc::Candidate candidate) {
		// 将 Candidate 发送给信令服务器
		QJsonObject json;
		json["type"] = "ICE";
		json["from"] = QString::fromStdString(m_myId);//����from�ֶ�
		json["to"] = QString::fromStdString(m_peerId);
		QJsonObject data;
		data["candidate"] = QString::fromStdString(candidate.candidate());
		data["sdpMid"] = QString::fromStdString(candidate.mid());
		data["sdpMLineIndex"] = candidate.port().value();
		json["data"] = data;

		QJsonDocument doc(json);
		if (m_ws && m_ws->isOpen()) {
			m_ws->send(doc.toJson(QJsonDocument::Compact).toStdString());//������Ϣ
		}
	});
	m_pc->onStateChange([this](rtc::PeerConnection::State state) {
		qDebug() << "PeerConnection State" << (int) state;
		if (state == rtc::PeerConnection::State::Connected) {
			emit stateChange("Connected");
		}
	});
	// 4. 建立信令连接
	setupSignaling(signalUrl);
}
void StreamReceiver::stop() {
	CleanupDecoder();
	if (m_ws) {
		m_ws->close();
		m_ws = nullptr;
	}
	emit stateChange("stopped");
}

bool StreamReceiver::ensureDecoder(AVCodecID codecId) {
	return initDecoder(codecId);
}

bool StreamReceiver::initDecoder(AVCodecID codecId) {
    if (m_isDecoderInited)
        return true;
    m_codec = avcodec_find_decoder(codecId);
    if (!m_codec) {
        CRITICAL() << "H264 decoder not found";
        return false;
    }
    m_codecCtx = avcodec_alloc_context3(m_codec);
    if (!m_codecCtx) {
        CRITICAL() << "Failed to allocate codec context";
        return false;
    }
    // 直接打开解码器，不使用 parser
    int ret = avcodec_open2(m_codecCtx, m_codec, nullptr);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        CRITICAL() << "Failed to open codec:" << errbuf;
        return false;
    }
    m_isDecoderInited = true;
    DEBUG() << "Decoder initialized successfully";
    return true;
}

void StreamReceiver::CleanupDecoder() {
    if (m_codecCtx) {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    m_isDecoderInited = false;
    DEBUG() << "Decoder cleaned up";
}

void StreamReceiver::onTrackData(const std::vector<std::byte>& data) {
    if (!m_isDecoderInited) {
        WARNING() << "Decoder not initialized";
        return;
    }

    const uint8_t* rawData = reinterpret_cast<const uint8_t*>(data.data());
    int rawSize = static_cast<int>(data.size());

    // 直接用 Annex-B 数据创建 packet
    m_packet->data = (uint8_t*)rawData;
    m_packet->size = rawSize;

    // 发送到解码器
    int ret = avcodec_send_packet(m_codecCtx, m_packet);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
        WARNING() << "Send packet failed:" << errbuf;
        return;
    }
    DEBUG() << "  Packet sent to decoder";

    // 接收解码帧
    int frameCount = 0;
    while ((ret = avcodec_receive_frame(m_codecCtx, m_frame)) >= 0) {
        frameCount++;
        emit frameReady(
            m_frame->data[0], m_frame->data[1], m_frame->data[2],
            m_frame->linesize[0], m_frame->linesize[1], m_frame->linesize[2],
            m_frame->width, m_frame->height);
    }
    
    if (frameCount == 0) {
        if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            WARNING() << "Receive frame error:" << errbuf;
        } else {
            DEBUG() << "  No complete frame yet (waiting for more data)";
        }
    }
}

void StreamReceiver::setupSignaling(const std::string& url) {
	// 1. 收到 answer 后，绑定 onLocalDescription，发送 SDP 到信令服务器
	if (m_pc) {
		m_pc->onLocalDescription([this](rtc::Description desc) {
			qDebug() << "Local Description(Answer) Generated,sending ...";
			QJsonObject json;
			if (desc.typeString() == "answer") {
				json["type"] = "ANSWER";
			}
			else if (desc.typeString() == "offer") {
				json["type"] = "OFFER";
			}
			else {
				json["type"] = (QString::fromStdString(desc.typeString())).toUpper();
			}
			json["from"] = QString::fromStdString(m_myId);
			json["to"] = QString::fromStdString(m_peerId);

			QJsonObject data;
			data["sdp"] = QString::fromStdString(std::string(desc));
			json["data"] = data;

			QJsonDocument doc(json);
			if (m_ws && m_ws->isOpen()) {
				m_ws->send(doc.toJson(QJsonDocument::Compact).toStdString());
			}
		});
	}

	// 2. 建立 WebSocket，注册并监听信令事件
	m_ws = std::make_shared<rtc::WebSocket>();
	m_ws->onOpen([this](){
		qDebug() << "WebSocket Connected!";
		//�����������������ص�������Ϣ����Ҫ������޸ģ�
		QJsonObject json;
		json["type"] = "REGISTER_REQUEST";
		json["to"] = "SERVER";
		QJsonDocument doc(json);
		m_ws->send(doc.toJson(QJsonDocument::Compact).toStdString());
	});

	// 3. 处理信令消息，转发到 WebRTC
	m_ws->onMessage([this](rtc::message_variant data) {
		if (!std::holds_alternative<std::string>(data)) return;
		if (std::holds_alternative<std::string>(data)) {
			std::string str = std::get<std::string>(data);
			//JSON ����
			QJsonParseError parserError;
			QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(str), &parserError);
			if (parserError.error != QJsonParseError::NoError || !doc.isObject()) {
				qWarning() << "Ignored invalid JSON signaling message";
				return;
			}

			QJsonObject json = doc.object();
			QString type = json["type"].toString();
			//3.1.���� SDP Offer
			if (type == "offer") {
				qDebug() << "Received Offer";
				std::string sdp = json["sdp"].toString().toStdString();
				//��Զ�˴����SDP�����ڱ���
				m_pc->setRemoteDescription(rtc::Description(sdp,"offer"));
				//����Answer����
				if(m_pc){
					m_pc->setLocalDescription();//���ɱ��ص�SDP����
				}
			}
			//3.2.���� ICE Candidate���յ�Զ�˵�ICE��ѡ��ַ��
			else if (type == "candidate") {
				if (!m_pc || !m_pc->remoteDescription().has_value()) return;

				std::string candidate = json["candidate"].toString().toStdString();
				std::string mid = json["sdpMid"].toString().toStdString();
				m_pc->addRemoteCandidate(rtc::Candidate(candidate, mid));
			}
		}
	});

	m_ws->onError([](std::string s) {
		qCritical() << "Web Socket Error:" << QString::fromStdString(s);
	});
	m_ws->onClosed([]() {
		qDebug() << "Web Socket Closed";
	});
	// 4. 打开 WebSocket 连接
	m_ws->open(url);
}

