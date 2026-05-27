# BytesScreenShare (局域网屏幕共享软件)

一款基于 Qt 6 开发的高性能局域网 P2P 屏幕共享与视频会议应用。项目利用 WebRTC (`libdatachannel`) 进行 P2P 穿透与连接，并结合原生 FFmpeg 实现了自定义的高效音视频流编解码与传输系统，具有极低的延迟与极其出色的画面表现能力。

## ✨ 技术亮点

### 1. 🚀 高效的屏幕捕获与流媒体处理
* **现代原生采集**：采用 Qt 6 `QMediaCaptureSession` 与 `QScreenCapture` 结合进行高帧率屏幕画面采集与推流。
* **硬核 FFmpeg 编解码**：
  * **编码端**：利用 FFmpeg (`libavcodec`, `libswscale`) 将捕获的原始图像帧 (RGB/YUV) 实时转换为 `H.264` 格式码流（默认 1080p, 30fps）。
  * **解码端**：接收到分片数据后还原结构，利用 FFmpeg 快速解析 NALU 单元解码回 YUV420P 数据格式。

### 2. 🌐 轻量级 WebRTC 与 WebSocket 信令
* **P2P 无缝连接**：抛弃了庞大复杂的原生 WebRTC 源码库，轻量级引入 `libdatachannel` 处理 ICE 穿透和底层连接（`rtc::PeerConnection`）。
* **自研媒体通道拆包传输**：在 `rtc::DataChannel` 数据通道上，从 0 到 1 构建了 RTP/H.264 视频流的压缩打包、FU-A 切片与分发系统，脱离传统轨道约束。
* **自定义 JSON 信令**：基于 `QWebSocket` 实现了高可用的信令中转服务器（承载 `OFFER`, `ANSWER`, `ICE` 交换逻辑与连接广播）。

### 3. 🎮 OpenGL 硬件极致加速渲染
* **零延迟三纹理渲染**：丢弃低效高负荷的 CPU 色彩转换，使用 `QOpenGLWidget` + `QOpenGLShaderProgram` 组建了直通 GPU 的自研渲染管线。
* 将网络拉取并解码出的 `Y`、`U`、`V` 分量数据作为独立纹理并行上传，利用 GLSL 片元着色器 (Fragment Shader) 在显卡硬件端直接进行 `YUV420p` 转 `RGB` 制图回显，将渲染带来的 CPU 压力降到极致。

### 4. ⚙️ 合理的多线程及高并发架构
* 恰到好处地实现了 UI 事件循环与音视频数据处理线程的分离：
  * **`VideoWorker`**：驻留在独立的工作线程中，接管高负载的屏幕采样、重采样滤波以及 H.264 视音频编码。
  * **`RenderWorker`**：独立出解码流管线，专门负责大量数据包提取、RTP 网络包防丢包组装以及解码。
* 跨线程安全通信：运用了 Qt 强大的 `Signals & Slots` (队列连接模式)，确保音视频高速收发期间主界面不会卡顿。

## 🎯 功能特点

- **无缝切换数据源**：支持一键切换屏幕流或者独立摄像头流，局域网同屏帧率拉满。
- **实时多人文本聊天**：内嵌文字交流面板，支持弹幕/常规信息与未读小红点标记，全方位实时互动监控。
- **会议本地录制**：深层联动 `QMediaRecorder` 支持音视频文件的本地录制一键启停。
- **原生应用级快捷键体验**：集成快捷键动作调度，如 `Ctrl+S` （共享页面）、`Ctrl+H` （呼出/隐藏聊天）、空格键 “按住以说话”等丝滑操作方案。

## 📂 项目核心结构与模块清单

* **`src/Capture/`**：封装 Qt6 屏幕及多媒体采集源驱动逻辑、帧分发队列缓冲服务。
* **`src/encoder/`**：FFmpeg `libavcodec` H.264 上层对象与自定义 RTP 封包算法库（网络包切片与封装）。
* **`src/render/`**：对接网络层 DataChannel 拉取视频流的重组器单元与 `QOpenGLWidget` YUV 硬件显示层。
* **`src/rtc/`**：`libdatachannel` WebRTC 对象生命周期管理，抽象 `PeerConnection`（SDP 会话描述和 ICE 解析集成）。
* **`src/signaling/`**：轻耦合 WebSocket 状态机信令同步系统。
* **`src/ui/`**：Qt Widget 视觉设计驱动与控制器交互逻辑引擎。

## 🛠️ 构建与运行指南

### 前置环境
* **Qt** 6.5+ (包含核心库与 QMedia 多媒体套件)
* **FFmpeg** 库
* **libdatachannel** 库
* **C++17** 支持的现代编译器

### 详细指南
见[环境配置](./Environment.md)
