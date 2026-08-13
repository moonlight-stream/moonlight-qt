# Moonlight V+ for PC

[中文](README.md)

[![Build](https://img.shields.io/github/actions/workflow/status/qiin2333/moonlight-qt/build.yml?branch=master)](https://github.com/qiin2333/moonlight-qt/actions/workflows/build.yml?query=branch%3Amaster)
[![Downloads](https://img.shields.io/github/downloads/qiin2333/moonlight-qt/total)](https://github.com/qiin2333/moonlight-qt/releases)

Moonlight V+ for PC is an enhanced desktop client based on [moonlight-stream/moonlight-qt](https://github.com/moonlight-stream/moonlight-qt), designed to work closely with [Foundation Sunshine](https://github.com/qiin2333/Sunshine).

It remains compatible with upstream Moonlight and standard Sunshine hosts, while improving the Foundation Sunshine desktop experience with clearer capability negotiation, finer quality/performance controls, and more efficient in-stream actions.

## Downloads

Download Windows, macOS, Linux AppImage, and Steam Link builds from [GitHub Releases](https://github.com/qiin2333/moonlight-qt/releases).

> **macOS currently ships Apple Silicon (arm64) builds only**, named like `Moonlight-VPlus-<version>-arm64.dmg`.
>
> **Linux AppImages are available for x86_64 and aarch64**, named like `Moonlight-VPlus-<version>-x86_64.AppImage` and `Moonlight-VPlus-<version>-aarch64.AppImage`.

For upstream Moonlight distribution channels, mobile clients, Flatpak, Snap, or distro packages, see the [Moonlight website](https://moonlight-stream.org) and the [upstream repository](https://github.com/moonlight-stream/moonlight-qt). Those builds may not include the Foundation Sunshine extensions maintained in Moonlight V+ for PC.

## Foundation Sunshine Integration

Foundation Sunshine is the primary server counterpart for Moonlight V+ for PC. The client probes host capabilities during connection setup; enhanced protocols are enabled only when the server advertises support, and standard Moonlight / Sunshine behavior is used otherwise.

You can use it like a regular Moonlight client, or pair it with a Foundation Sunshine host for richer clipboard, audio input, display control, bitrate control, and folder-mapping behavior. When an extension is unavailable, the client falls back to standard compatible behavior.

## Key Enhancements

### Protocol And Streaming

- **Bidirectional clipboard sync**: supports text and PNG image sync between the client and Foundation Sunshine host, including common browser and native clipboard formats.
- **High-quality microphone forwarding**: uses the microphone extension in `moonlight-common-c` for continuous audio input and multichannel scenarios.
- **Remote resolution decoupling**: stream resolution can be independent from the local display resolution, with custom remote resolution and frame-rate controls.
- **AppView display control**: supports target display selection, virtual display groups, remote resolution, and remote frame-rate preferences so the client and Foundation Sunshine share the same display intent.
- **Folder mapping / host file access**: exposes Foundation Sunshine folder-mapping features in the client, including in-stream access to shared host files and cross-platform mount support.

### Quality And Performance

- **Sunshine ABR**: when supported by the host, Foundation Sunshine can adjust session bitrate dynamically from client feedback, balancing clarity and stability more effectively.
- **High-bitrate LAN streaming**: keeps Sunshine-oriented high-bitrate controls for wired LAN, desktop streaming, and high-resolution sessions.
- **Hardware decoding and modern video formats**: inherits upstream Moonlight hardware decoding support, including H.264, HEVC, AV1, HDR, and YUV 4:4:4 combinations depending on the client GPU and host encoder.
- **Frame pacing and latency controls**: keeps V-Sync, frame pacing, fullscreen, and borderless-window choices for tuning latency, smoothness, and desktop workflows.
- **Performance overlay**: exposes real-time stream metrics with additional render-time visibility, making stream tuning more measurable.

### Client Experience

- **Windows 11 style floating menu** with animation, icons, and an optional floating moon button.
- **Floating menu quick controls** for fullscreen, performance stats, mouse mode, cursor visibility, microphone, host file access, and other common in-stream actions.
- **Gamepad improvements** including configurable quit combos, instant gamepad/mouse switching, and context-aware settings visibility to reduce unrelated settings noise.
- **Remote desktop mouse mode** alongside game-style pointer capture, covering games, desktop work, and quick maintenance sessions.
- **Automatic IME suppression while streaming** on Windows via Win32 IMM hooks to improve keyboard input stability.
- **AppView presentation** for running state and display options, making it clearer where a session will launch.

### Automation And Releases

- **Fork-specific update checks** using GitHub Releases from `qiin2333/moonlight-qt`.
- **Git tag based versioning** through `scripts/derive-version.py`, keeping CI and local artifact names consistent.
- **Automated translation build** via `.github/workflows/build-translate.yml` for `.ts` / `.qm` resources.
- **CI build matrix** for Windows, macOS, Linux AppImage, and Steam Link artifacts.

## Compatibility

- The standard Moonlight / Sunshine protocol remains compatible, so regular hosts do not need extra setup.
- When connected to standard Sunshine or another upstream-compatible host, Foundation extensions such as image clipboard, HQ Mic, ABR, and folder mapping gracefully downgrade or stay disabled.
- NVIDIA GameStream compatibility is inherited from upstream Moonlight, but new development in Moonlight V+ for PC is primarily focused on the Sunshine ecosystem.

## Building

### Common Setup

```powershell
git submodule update --init --recursive
```

Windows and macOS builds also need prebuilt dependencies:

```powershell
.\setup-deps.ps1
```

On macOS, use:

```bash
python3 setup-deps.py
```

### Windows

Requirements:

- Qt 6 SDK, preferably close to the Qt 6.11.x version used by CI.
- Visual Studio 2022 with the MSVC toolchain.
- 7-Zip when building user-facing installers.
- Windows Graphics Tools when debugging DirectX-related issues.

Common release build scripts:

```cmd
scripts\build-arch.bat x64 release
scripts\generate-bundle.bat
```

### macOS

Requirements:

- Qt 6 SDK, preferably close to the Qt 6.11.x version used by CI.
- Xcode 14 or later.
- `create-dmg` when producing DMG artifacts.

Common build commands:

```bash
qmake6 moonlight-qt.pro
make release
scripts/generate-dmg.sh
```

### Linux

Requirements:

- Qt 6 is recommended; Qt 5.12 or later remains supported.
- GCC or Clang.
- FFmpeg 4.0 or later.
- The Vulkan renderer requires `libplacebo` v7.349.0 or later, and FFmpeg 6.1 or later is recommended.

Example Debian / Ubuntu dependencies:

```bash
sudo apt install libegl1-mesa-dev libgl1-mesa-dev libopus-dev libsdl2-dev libsdl2-ttf-dev libssl-dev libavcodec-dev libavformat-dev libswscale-dev libva-dev libvdpau-dev libxkbcommon-dev wayland-protocols libdrm-dev qt6-base-dev qt6-declarative-dev libqt6svg6-dev qt6-wayland
```

Development build:

```bash
qmake6 moonlight-qt.pro
make debug
```

### Steam Link

Requirements:

- Clone the [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk).
- Set the `STEAMLINK_SDK_PATH` environment variable.

Build:

```bash
scripts/build-steamlink-app.sh
```

Original Steam Link hardware limits:

- Maximum resolution: 1080p.
- Maximum frame rate: 60 FPS.
- Maximum video bitrate: 40 Mbps.
- HDR streaming is not supported.

## Contributing

Moonlight V+ for PC prioritizes Foundation Sunshine integration, desktop client experience, Chinese localization, and cross-platform build stability.

When opening an issue or pull request, please include:

- Client operating system and version.
- Foundation Sunshine or standard Sunshine version.
- Whether you are using a Moonlight V+ for PC release build.
- Whether the report involves an extension such as clipboard sync, HQ Mic, ABR, folder mapping, or remote resolution handling.

## Upstream And License

This project is based on [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt) and follows the [GPLv3 License](LICENSE) included in this repository.

Upstream Moonlight links:

- [Moonlight website](https://moonlight-stream.org)
- [Moonlight Qt](https://github.com/moonlight-stream/moonlight-qt)
- [Moonlight documentation](https://github.com/moonlight-stream/moonlight-docs/wiki)
- [Moonlight Discord](https://moonlight-stream.org/discord)
- [Moonlight Weblate](https://hosted.weblate.org/projects/moonlight/moonlight-qt/)

Thanks to the Moonlight, Sunshine, and related open-source dependency maintainers.
