# Artemis Qt

[Artemis Qt](https://github.com/wjbeckett/artemis) is an enhanced cross-platform client for NVIDIA GameStream and [Apollo](https://github.com/ClassicOldSong/Apollo)/[Sunshine](https://github.com/LizardByte/Sunshine) servers. It's a fork of [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt) that brings the advanced features from [Artemis Android](https://github.com/ClassicOldSong/moonlight-android) to desktop platforms.

**Perfect for Steam Deck and other handheld gaming devices!**

[![Build Status](https://github.com/wjbeckett/artemis/workflows/Build%20Artemis%20Qt/badge.svg)](https://github.com/wjbeckett/artemis/actions)
[![Downloads](https://img.shields.io/github/downloads/wjbeckett/artemis/total)](https://github.com/wjbeckett/artemis/releases)

## ✨ Artemis Features

Artemis Qt includes all the features of Moonlight Qt, plus these enhanced capabilities:

### 🎯 Phase 1 (Foundation) - In Development
- **📋 Clipboard Sync** - Seamlessly sync clipboard content between client and server
- **⚡ Server Commands** - Execute custom commands on the Apollo/Sunshine server
- **🔐 OTP Pairing** - One-Time Password pairing for enhanced security

### 🎮 Phase 2 (Client Controls) - Planned
- **🖥️ Fractional Refresh Rate** - Client-side control for custom refresh rates (e.g., 90Hz, 120Hz)
- **📐 Resolution Scaling** - Client-side resolution scaling for better performance
- **🖼️ Virtual Display Control** - Choose whether to use virtual displays

### 🚀 Phase 3 (Advanced) - Future
- **📱 App Ordering** - Custom app ordering without compatibility mode
- **🔍 Permission Viewing** - View and manage server-side permissions
- **🎯 Input-Only Mode** - Stream input without video for remote control scenarios

## 🎮 Perfect for Steam Deck

Artemis Qt is specifically optimized for handheld gaming devices like the Steam Deck:

- **Embedded Mode** - Optimized UI for handheld devices
- **GPU-Optimized Rendering** - Efficient rendering for lower-power GPUs
- **Touch-Friendly Interface** - Designed for touch and gamepad navigation
- **Power Efficient** - Optimized for battery life

## 📥 Downloads

### Latest Release
- **[Windows x64/ARM64](https://github.com/wjbeckett/artemis/releases/latest)** - Native Windows builds
- **[macOS Universal](https://github.com/wjbeckett/artemis/releases/latest)** - Intel and Apple Silicon support
- **[Linux AppImage](https://github.com/wjbeckett/artemis/releases/latest)** - Universal Linux compatibility
- **[Steam Deck Package](https://github.com/wjbeckett/artemis/releases/latest)** - Optimized for Steam Deck
- **[Flatpak](https://github.com/wjbeckett/artemis/releases/latest)** - Sandboxed Linux package

## 🎮 Moonlight Features (Inherited)
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - H.264, HEVC, and AV1 codec support (AV1 requires Sunshine and a supported host GPU)
 - YUV 4:4:4 support (Sunshine only)
 - HDR streaming support
 - 7.1 surround sound audio support
 - 10-point multitouch support (Sunshine only)
 - Gamepad support with force feedback and motion controls for up to 16 players
 - Support for both pointer capture (for games) and direct mouse control (for remote desktop)
 - Support for passing system-wide keyboard shortcuts like Alt+Tab to the host
 
## Downloads
- [Windows, macOS, and Steam Link](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Snap (for Ubuntu-based Linux distros)](https://snapcraft.io/moonlight)
- [Flatpak (for other Linux distros)](https://flathub.org/apps/details/com.moonlight_stream.Moonlight)
- [AppImage](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Raspberry Pi 4 and 5](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Raspberry-Pi-4)
- [Generic ARM 32-bit and 64-bit Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-ARM%E2%80%90based-Single-Board-Computers) (not for Raspberry Pi)
- [Experimental RISC-V Debian packages](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-RISC%E2%80%90V-Single-Board-Computers)
- [NVIDIA Jetson and Nintendo Switch (Ubuntu L4T)](https://github.com/moonlight-stream/moonlight-docs/wiki/Installing-Moonlight-Qt-on-Linux4Tegra-(L4T)-Ubuntu)

#### Special Thanks

[![Hosted By: Cloudsmith](https://img.shields.io/badge/OSS%20hosting%20by-cloudsmith-blue?logo=cloudsmith&style=flat-square)](https://cloudsmith.com)

Hosting for Moonlight's Debian and L4T package repositories is graciously provided for free by [Cloudsmith](https://cloudsmith.com).

## 🛠️ Building from Source

### Quick Start
```bash
# Clone the repository
git clone https://github.com/wjbeckett/artemis.git
cd artemis

# Run the development setup script
chmod +x scripts/setup-dev.sh
./scripts/setup-dev.sh

# The script will install dependencies and build the project
```

### Manual Build Requirements

#### All Platforms
- **Qt 6.7+** (Qt 6.8+ recommended)
- **FFmpeg 4.0+**
- **SDL2** and **SDL2_ttf**
- **OpenSSL**
- **Opus codec**

#### Platform-Specific Requirements

**Windows:**
- Visual Studio 2022 with MSVC
- 7-Zip (for packaging)

**macOS:**
- Xcode 14+
- Homebrew: `brew install qt6 ffmpeg opus sdl2 sdl2_ttf create-dmg`

**Linux:**
```bash
# Ubuntu/Debian
sudo apt install qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-templates \
  qml6-module-qtquick-layouts libegl1-mesa-dev libgl1-mesa-dev \
  libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev \
  libavcodec-dev libavformat-dev libswscale-dev libva-dev \
  libvdpau-dev libxkbcommon-dev wayland-protocols libdrm-dev

# Fedora/RHEL
sudo dnf install qt6-qtbase-devel qt6-qtdeclarative-devel \
  qt6-qtsvg-devel openssl-devel SDL2-devel SDL2_ttf-devel \
  ffmpeg-devel libva-devel libvdpau-devel opus-devel \
  pulseaudio-libs-devel alsa-lib-devel libdrm-devel
```

### Build Commands
```bash
# Initialize submodules
git submodule update --init --recursive

# Configure and build
qmake6 moonlight-qt.pro CONFIG+=release
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
nmake  # Windows
```

## 🎮 Features Comparison

| Feature | Moonlight Qt | Artemis Qt |
|---------|--------------|------------|
| GameStream/Sunshine Support | ✅ | ✅ |
| Hardware Video Decoding | ✅ | ✅ |
| HDR Streaming | ✅ | ✅ |
| 7.1 Surround Sound | ✅ | ✅ |
| Multi-touch Support | ✅ | ✅ |
| **Clipboard Sync** | ❌ | ✅ |
| **Server Commands** | ❌ | ✅ |
| **OTP Pairing** | ❌ | ✅ |
| **Fractional Refresh Rates** | ❌ | 🚧 |
| **Resolution Scaling** | ❌ | 🚧 |
| **Virtual Display Control** | ❌ | 🚧 |
| **Custom App Ordering** | ❌ | 📋 |
| **Permission Viewing** | ❌ | 📋 |
| **Input-Only Mode** | ❌ | 📋 |

Legend: ✅ Available, 🚧 In Development, 📋 Planned

## 🤝 Contributing

We welcome contributions! Here's how to get started:

1. **Fork the repository**
2. **Create a feature branch**: `git checkout -b feature/amazing-feature`
3. **Make your changes** and test thoroughly
4. **Commit your changes**: `git commit -m 'Add amazing feature'`
5. **Push to the branch**: `git push origin feature/amazing-feature`
6. **Open a Pull Request**

### Development Guidelines
- Follow the existing code style
- Add tests for new features
- Update documentation as needed
- Test on multiple platforms when possible

## 🔗 Related Projects

- **[Artemis Android](https://github.com/ClassicOldSong/moonlight-android)** - The original Artemis for Android
- **[Apollo Server](https://github.com/ClassicOldSong/Apollo)** - Enhanced GameStream server
- **[Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt)** - The upstream project
- **[Sunshine](https://github.com/LizardByte/Sunshine)** - Open-source GameStream server

## 📄 License

This project is licensed under the GPL v3 License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **[ClassicOldSong](https://github.com/ClassicOldSong)** - Creator of Artemis Android and Apollo server
- **[Moonlight Team](https://github.com/moonlight-stream)** - For the excellent foundation
- **[LizardByte](https://github.com/LizardByte)** - For the Sunshine server
- **All contributors** who help make this project better

---

**Made with ❤️ for the gaming community**
