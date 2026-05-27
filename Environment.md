# 开发环境配置

# 屏幕共享软件开发环境配置

- Compiler: MSVC v143 \(Visual Studio 2022, x64\)

- CMake: 3\.24\+

- Qt: 6\.8\.3 \(msvc2022\_64\)

- C\+\+ Standard: C\+\+17

## Visual Studio 2022

下载链接：[适用于 Windows、Mac 和 Linux 的 Visual Studio 和 VS Code 下载](https://visualstudio.microsoft.com/zh-hans/downloads/)

选择社区版即可满足开发需求\.



具体的安装流程可以参考微软官方文档或B站教程，见下：

- [Install Visual Studio and Choose Your Preferred Features \| Microsoft Learn](https://learn.microsoft.com/en-us/visualstudio/install/install-visual-studio?view=visualstudio)

- [【2025最新】全网最细Visual Studio2022下载安装使用教程，一键安装永久使用，手把手教你，包成功的\!\_哔哩哔哩\_bilibili](https://www.bilibili.com/video/BV1aWPjeKEbB/?spm_id_from=333.337.search-card.all.click&vd_source=34cbfe1dcedae7e26848b415ac9ce814)



#### 安装MSVC编译器

需要注意Step4中，需要选择**使用C\+\+的桌面开发**，并且在具体组件中必须勾选**MSVC**编译器：

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Y2JlMzc5OTliZmYwY2EwM2Y2ZGIyZTYyODNhNmFiMTRfMzIyYTJkNTg2YmU4MDNkODc1NmY2MTBhYWMyMWQzNTRfSUQ6NzU3NDc4Nzc0NzkwMzM0Mzg0MF8xNzc5ODgwMjQyOjE3Nzk5NjY2NDJfVjM)



如果使用VS进行后续开发，需要在VS中安装QT插件：

1. 选择**拓展**选项卡，点击**管理拓展**

2. 搜索`QT`，直接安装**Qt VS Tools**

3. 在插件选项中具体配置QT的安装目录，需要选择使用MSVC编译器的目录

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=NWM2ZjE5Mzk0NzM5ZmVkNjM5NjVkODM0ZTA2MjE4ZjNfNGQ0ZjBlMjE1NGY5OTc3MWQ3NDYxNGE0ODA5M2ExZDNfSUQ6NzU3NDc4Nzc0NjUzMjI2NDkxOV8xNzc5ODgwMjQyOjE3Nzk5NjY2NDJfVjM)



参考视频见下：

[如何配置visual studio 2022 使之可以开发Qt应用\_哔哩哔哩\_bilibili](https://www.bilibili.com/video/BV1twpSeNEPn/?spm_id_from=333.337.search-card.all.click&vd_source=34cbfe1dcedae7e26848b415ac9ce814)


## QT

版本选择：

- 6\.8\.3版本（当前使用版本）

*TIP：QT在6\.5版本之后才加入屏幕屏幕采集的特性。如果视频流的采集使用WebRTC，那么可以使用5\.14版本。*

下载链接：[试用 Qt \| 开发应用程序和嵌入式系统 \| Qt](https://www.qt.io/zh-cn/download-dev)

安装教程：https://zhuanlan\.zhihu\.com/p/697911596

安装的时候与VS安装时一样要注意编译器的选择，如果单纯使用QT开发，选择MinGW，否则需要选择MSVC2022\. 当然，建议两个一起安装也可以。

> [02 Qt下载与安装\_哔哩哔哩\_bilibili](https://www.bilibili.com/video/BV1D5GPzkER5/?spm_id_from=333.1391.0.0&p=2&vd_source=34cbfe1dcedae7e26848b415ac9ce814)
> 
> 



## WebRTC

### 删减WebRTC库

#### [libdatachannel](https://github.com/paullouisageneau/libdatachannel)

官方的Build文档：

[libdatachannel/BUILDING\.md at master · paullouisageneau/libdatachannel](https://github.com/paullouisageneau/libdatachannel/blob/master/BUILDING.md)



**编译准备**

需要安装1\.1\.1\_x64的OpenSSL。（亲测高版本的反而会有问题）一定要安装64bit版本

下载连接：[百度网盘](https://pan.baidu.com/share/init?surl=SLhNDmRC-tmm86lDnEvNmw&pwd=abcd)



如果之前构建过库，需要清除Build文件夹，可以运行以下命令或者直接删除

```Shell
rd /s /q build
```



#### 工程构建方法一

1. 首先将远程克隆仓库放到本地（为方便项目管理，可以考虑在项目根目录创建一个`third\_party`文件夹）

2. 在开始菜单中找到VS的命令窗口（接下来将使用VS的编译器对库进行编译）

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=OGNkMWEyYTY1ODViNTQyYzVmMDBhYWVlNzM0YzZhMzFfNzExNGNlOGE0MDA3YTEzZGJiOWE0Yjg4MzNkMTgxODZfSUQ6NzU3NjY2NDYyNDM0Mzc4MDMxMF8xNzc5ODgwMjQyOjE3Nzk5NjY2NDJfVjM)

3. 使用`cd`命令转到仓库的根目录，并运行如下cmake命令，生成**build**文件夹。

```Shell
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
or
cmake -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
```

4. 进入到build文件夹，再运行下面的命令，完成编译。

```Shell
nmake
```

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=Njc1MTk5NDZmYzMzZTA1NTRlODk2NWM2MDVkNzQzMjdfMTZhNzBjYWY2ODhjNGQyNmE1NDhjOWFjMjA0OGM1YmNfSUQ6NzU3NjY2MzMzNzU2ODk0NzQxOV8xNzc5ODgwMjQyOjE3Nzk5NjY2NDJfVjM)


#### 构建成功的标志

如果在/build目录下生成`\.exe`可执行文件，并且能够运行，说明编译成功

![Image](https://internal-api-drive-stream.feishu.cn/space/api/box/stream/download/authcode/?code=MmQ0ZWNjZTI2ZjRiZTQzOWRhYTBkMWViMjE0ZmNiNTZfMWU4YjNjNmEzNjQ3YTg0MWQ3YzQ5MWI0YTczZjhkOGRfSUQ6NzU3NjYzMTgxNDU0NzExNTIyOV8xNzc5ODgwMjQyOjE3Nzk5NjY2NDJfVjM)

#### VS中添加配置WebRTC库

如果正确构建了64位平台的库，可以按照博客https://blog\.csdn\.net/Aritro/article/details/131833601，导入到VS的环境中。


可以构建一个基础QT项目，引入头文件并随便定义一个库内的变量验证是否成功。

示例：

```C++
// in TestForWebRTC.cpp
#include "TestForWebRTC.h"
#include <QLabel>
#include "rtc/rtc.hpp"

TestForWebRTC::TestForWebRTC(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    QLabel* qlabelPtr = new QLabel(this);
    qlabelPtr->setText("test WebRTC");
    rtc::WebSocket ws; // WebRTC库中的类
}

TestForWebRTC::~TestForWebRTC()
{}
```

## ffmpeg安装与编译

在获取视频流后要将视频压缩为h264编码并变为RTP数据包发送给libdatachannel进行传输，这里需要用到ffmpeg工具。安装教程如下：

https://blog\.csdn\.net/evm\_doc/article/details/145808750

注意同样需要加入环境变量（加入后需要重启Visual Studio）

大部分问题依然可以在项目属性\-C/C\+\+\-常规、\.\.\.\-链接器\-输入\-附加库目录/附加依赖库中添加。此处可以同时检查Qt的环境配置（直接使用Qt vs tool似乎会无法找到Qt6Core\.lib\)

## 环境测试与成功标志

可以使用以下代码测试：

```C++
#include <iostream>

// --- Qt 检查 ---
// 确保在 VS 项目属性 -> C/C++ -> 常规 -> 附加包含目录 中加入了 Qt 的 include 路径
// 确保在 链接器 -> 输入 -> 附加依赖项 中加入了 Qt6Core.lib
#include <QtCore/QCoreApplication>
#include <QtCore/QString>

// --- FFmpeg 检查 ---
// 确保在 附加包含目录 中加入了 ffmpeg/include
// 确保在 附加依赖项 中加入了 avcodec.lib, avutil.lib, avformat.lib, swscale.lib
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

// --- libdatachannel 检查 ---
// 确保在 附加包含目录 中加入了 libdatachannel/include
// 确保在 附加依赖项 中加入了 datachannel.lib (以及它依赖的 ssl 等库)
#include <rtc/rtc.hpp>

int main(int argc, char* argv[]) {
    std::cout << "========== Environment Check Start ==========" << std::endl;
    std::cout.flush();  // 立即刷新缓冲区

    // 1. 验证 Qt 环境（不创建 QCoreApplication）
    try {
        // 直接调用 qVersion()，无需创建应用对象
        const char* qtVersionStr = qVersion();
        std::cout << "[SUCCESS] Qt Version: " << qtVersionStr << std::endl;
        std::cout.flush();
    }
    catch (...) {
        std::cerr << "[ERROR] Qt version check failed!" << std::endl;
        std::cerr.flush();
    }

    // 2. 验证 FFmpeg 环境
    unsigned version = avcodec_version();
    std::cout << "[SUCCESS] FFmpeg libavcodec version: " << version << std::endl;
    std::cout.flush();

    SwsContext* sws = sws_alloc_context();
    if (sws) {
        std::cout << "[SUCCESS] FFmpeg swscale is ready." << std::endl;
        sws_freeContext(sws);
    }
    else {
        std::cerr << "[ERROR] FFmpeg swscale failed!" << std::endl;
    }
    std::cout.flush();

    // 3. 验证 libdatachannel 环境
    try {
        rtc::Configuration config;
        std::cout << "[SUCCESS] libdatachannel initialized." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] libdatachannel init failed: " << e.what() << std::endl;
    }
    std::cout.flush();

    std::cout << "========== All Checks Passed ==========" << std::endl;
    std::cout.flush();

    return 0;
}
```

已经在本地运行测试过，输出为：

```Plain Text
========== Environment Check Start ==========
[SUCCESS] Qt Version: 6.8.3
[SUCCESS] FFmpeg libavcodec version: 4066148
[SUCCESS] FFmpeg swscale is ready.
[SUCCESS] libdatachannel initialized.
========== All Checks Passed ==========
```