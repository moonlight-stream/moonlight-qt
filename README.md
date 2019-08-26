# Moonlight PC

[Moonlight PC](https://moonlight-stream.org) is an open source implementation of NVIDIA's GameStream, as used by the NVIDIA Shield, but built to run on Windows, Mac, and Linux. This client is the successor to [Moonlight Chrome](https://github.com/moonlight-stream/moonlight-chrome) for streaming on PC.

Moonlight also has mobile versions for [Android](https://github.com/moonlight-stream/moonlight-android) and  [iOS](https://github.com/moonlight-stream/moonlight-ios).

You can follow development on our [Discord server](https://discord.gg/6ERtzFY).

 [![Windows AppVeyor Status](https://ci.appveyor.com/api/projects/status/glj5cxqwy2w3bglv/branch/master?svg=true)](https://ci.appveyor.com/project/cgutman/moonlight-qt/branch/master)
 [![Mac and Linux Travis CI Status](https://travis-ci.org/moonlight-stream/moonlight-qt.svg?branch=master)](https://travis-ci.org/moonlight-stream/moonlight-qt)
 [![Downloads](https://img.shields.io/github/downloads/moonlight-stream/moonlight-qt/total)](https://github.com/moonlight-stream/moonlight-qt/releases)

## Features
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - Supports streaming at up to 120 FPS (high refresh rate monitor recommended)
 - Supports streaming at 720p, 1080p, 1440p, 4K, and the client PC's native screen resolution
 - 5.1 surround sound audio
 - HEVC support for better image quality at reduced bandwidth
 - Keyboard and mouse support
 - Gamepad support with force feedback for up to 4 players
 
## Downloads
- [Windows, macOS, and Steam Link](https://github.com/moonlight-stream/moonlight-qt/releases)
- [Snap (for Ubuntu-based Linux distros)](https://snapcraft.io/moonlight)
- [Flatpak (for other Linux distros)](https://flathub.org/apps/details/com.moonlight_stream.Moonlight)

## Building
### General Build Requirements
* Qt 5.9 SDK or later

### Windows-specific Build Requirements
* Windows 7 or later
* [Visual Studio](https://visualstudio.microsoft.com/downloads/) 2017 (Community edition is fine)
* Select MSVC Desktop toolchain during Qt installation
* [DirectX SDK](https://www.microsoft.com/en-us/download/details.aspx?id=6812)
* [7-Zip](https://www.7-zip.org/) (only if building installers for non-development PCs)
* [WiX Toolset](http://wixtoolset.org/releases/) v3.11 or later (only if building installers for non-development PCs)

### Mac-specific Build Requirements
* macOS Sierra (10.12) or later
* Xcode 10
* [create-dmg](https://github.com/sindresorhus/create-dmg) (only if building DMGs for use on non-development Macs)

### Linux-specific Build Requirements
* GCC or Clang
* Install your distro equivalents of: `openssl-devel qt5-devel SDL2-devel ffmpeg-devel qt5-qtquickcontrols2-devel libva-devel libvdpau-devel opus-devel pulseaudio-libs-devel alsa-lib-devel SDL2_ttf-devel`
* FFmpeg 4.0 is required to build. If your distro doesn't package FFmpeg 4.0, you can build and install it from source on http://ffmpeg.org/

### Steam Link-specific Build Requirements
* [Steam Link SDK](https://github.com/ValveSoftware/steamlink-sdk) cloned on your build system
* STEAMLINK_SDK_PATH environment variable set to the Steam Link SDK path

### Build Setup Steps
1. Install the latest Qt SDK (and optionally, the Qt Creator IDE) from https://www.qt.io/download
    * You can install Qt via Homebrew on macOS, but you will need to use `brew install qt --with-debug` to be able to create debug builds of Moonlight.
    * You may also use your Linux distro's package manager for the Qt SDK as long as the packages are Qt 5.9 or later.
    * This step is not required for building on Steam Link, because the Steam Link SDK includes Qt 5.9.
2. Run `git submodule update --init --recursive` from within `moonlight-qt/`
3. Open the project in Qt Creator or build from qmake on the command line.
    * To build a binary for use on non-development machines, use the scripts in the `scripts` folder.
        * For Windows builds, use `scripts\generate-installers.bat`. Execute this script from the root of the repository within a Qt command prompt. Ensure WiX and 7-Zip binary directories are in your `%PATH%`.
        * For macOS builds, use `scripts/generate-dmg.sh`. Execute this script from the root of the repository and ensure Qt's `bin` folder is in your `$PATH`.
        * For Steam Link builds, run `scripts/build-steamlink-app.sh` from the root of the repository.
    * To build from the command line for development use, run `qmake moonlight-qt.pro` then `make debug` or `make release`

## Contribute
1. Fork us
2. Write code
3. Send Pull Requests

Check out our [website](https://moonlight-stream.org) for project links and information.
