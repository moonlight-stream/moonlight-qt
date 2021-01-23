# Moonlight PC 中文版
本项目为 Moonlight PC 的简体中文版本，只对简体中文语言文件进行更新和维护。

## 简体中文使用方法
1. 从 [原仓库](https://github.com/moonlight-stream/moonlight-qt/releases) 下载并安装 Moonlight PC 的英文版本
2. 从本仓库下载 [简体中文语言文件](https://github.com/WLongSAMA/moonlight-qt_zh-CN/raw/master/app/languages/qml_zh_cn.qm)
3. 将语言文件重命名为 `qt_zh_CN.qm`
4. 将 qt_zh_CN.qm 复制到 Moonlight 所在文件夹中的 translations 文件夹里

# Moonlight PC

[Moonlight PC](https://moonlight-stream.org) 是 NVIDIA GameStream 的一个开源实现，它基于 NVIDIA SHIELD，但它可以在 Windows、Mac 以及 Linux 平台上运行。这个客户端是 [Moonlight Chrome](https://github.com/moonlight-stream/moonlight-chrome) 在 PC 上的继承者。

Moonlight 还有 [Android](https://github.com/moonlight-stream/moonlight-android) 和 [iOS](https://github.com/moonlight-stream/moonlight-ios) 平台的版本。

你可以通过 [Discord 服务](https://moonlight-stream.org/discord) 关注我们的开发进程。

 [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/glj5cxqwy2w3bglv/branch/master?svg=true)](https://ci.appveyor.com/project/cgutman/moonlight-qt/branch/master)
 [![Downloads](https://img.shields.io/github/downloads/moonlight-stream/moonlight-qt/total)](https://github.com/moonlight-stream/moonlight-qt/releases)

## 特点
 - Windows、Mac 和 Linux 平台上的硬件加速视频解码
 - 支持高达 120 FPS 的流式传输（建议使用高刷新率显示器）
 - 支持 720p、1080p、1440p、4K 的画面，以及客户端 PC 的本机屏幕分辨率
 - 支持 7.1 环绕声
 - 支持指针捕捉（游戏）和直接鼠标控制（远程桌面）
 - 支持在全屏模式下向主机传递系统范围的键盘快捷键，如 Alt+Tab
 - 直接触摸屏鼠标控制
 - HEVC 支持以更低的带宽获得更好的图像质量
 - 支持最多 4 名玩家同时使用手柄控制
 
## 下载
- 适用于 [Windows，macOS 以及 Steam Link](https://github.com/moonlight-stream/moonlight-qt/releases) 的版本
- [Snap（基于 Ubuntu 的 Linux 发行版）](https://snapcraft.io/moonlight)
- [Flatpak（其他 Linux 发行版）](https://flathub.org/apps/details/com.moonlight_stream.Moonlight)
- [AppImage](https://github.com/moonlight-stream/moonlight-qt/releases)
- 适用于 [树莓派 4](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Raspberry-Pi-4) 的版本
- 适用于 [NVIDIA Jetson 和任天堂 Switch (Ubuntu L4T)](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Linux4Tegra-(L4T)-Ubuntu) 的版本

## 构建

### Windows 平台下构建所需的引用
* QT 5.15 SDK 或者更高的版本
* Windows 7 或者更高的版本
* [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/) （社区版本即可）
* 在安装 QT 的时候选择**MSVC 2019**选项。不支持 MinGW。
* [7-Zip](https://www.7-zip.org/)（仅在构建 PC 版安装程序的时候使用）
* [WiX Toolset](https://wixtoolset.org/releases/) v3.11 版本或者更高（仅在构建 PC 版安装程序的时候使用）

### macOS 平台下构建所需的引用
* QT 5.15 SDK 或者更高的版本
* macOS High Sierra (10.13) 或者更高的版本
* Xcode 11
* [create-dmg](https://github.com/sindresorhus/create-dmg) （仅在构建 DMGs 的时候使用）

### Linux/Unix 平台下构建所需的引用
* QT 5.9 SDK 或者更高的版本
* GCC 编译器或者 Clang 编译器
* 安装依赖包：
  * Debian/Ubuntu: `libegl1-mesa-dev libgl1-mesa-dev libopus-dev libqt5svg5-dev libsdl2-dev libsdl2-ttf-dev libssl-dev libavcodec-dev libva-dev libvdpau-dev libxkbcommon-dev qt5-default qt5-qmake qtbase5-dev qtdeclarative5-dev qtquickcontrols2-5-dev wayland-protocols qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qtquick-window2 qml-module-qtquick2`
  * RedHat/Fedora: `openssl-devel qt5-devel SDL2-devel ffmpeg-devel qt5-qtquickcontrols2-devel libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel alsa-lib-devel SDL2_ttf-devel`
* 构建时需要 FFmpeg 4.0+。如果你的发行版没有安装 FFmpeg 4.0 或更高的版本，你可以从 https://ffmpeg.org/ 下载源码编译并安装它。

### Steam Link 平台下构建所需的引用
* [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) 从当前系统中克隆
* 将 STEAMLINK_SDK_PATH 设置为 Steam Link SDK 的安装路径

### 构建步骤
1. 从 https://www.qt.io/download 下载并安装最新版本的 QT SDK（以及可选的 QT Creator IDE）
    * 在 macOS 中可以通过 Homebrew 安装 QT，但需要使用 `brew install qt --with-debug` 才能创建 Moonlight 的调试版本。
    * 你也可以使用 Linux 发行版的软件包管理器来安装 QT SDK，只要是 QT 5.9 或更高版本的软件包即可。
    * 构建 Steam Link 的版本不需要此步骤，因为 Steam Link SDK 已经包含 QT 5.9。
2. 在 `moonlight-qt/` 目录中运行 `git submodule update --init --recursive`
3. 通过 QT Creator 打开项目或在命令行中运行 qmake。
    * 要构建在非开发机器上使用的二进制文件，请使用“scripts”文件夹中的脚本。
        * 对于 Windows 版本，使用 `scripts\build-arch.bat` 和 `scripts\generate-bundle.bat`。在 QT 命令提示符下在仓库的根目录执行这些脚本。确保 WiX 和 7-Zip 的二进制文件所在目录已添加到 `%PATH%` 中。
        * 对于 macOS 版本，使用 `scripts/generate-dmg.sh`。从仓库的根目录执行这个脚本，并确保 QT 的 `bin` 文件夹所在路径已添加到 `$PATH` 中。
        * 对于 Steam Link 版本，从仓库的根目录执行 `scripts/build-steamlink-app.sh`。
    * 若要从命令行编译，运行 `qmake moonlight-qt.pro` 然后运行 `make debug` 或 `make release`

## 贡献
1. Fork 这个项目
2. 编写代码
3. 发送 Pull Requests

欢迎到我们的 [网站](https://moonlight-stream.org) 查看项目链接和信息。
