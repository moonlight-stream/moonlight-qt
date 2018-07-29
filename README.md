# Moonlight PC

[Moonlight PC](http://moonlight-stream.com) is an open source implementation of NVIDIA's GameStream, as used by the NVIDIA Shield, but built to run on Windows, Mac, and Linux. This client is the successor to [Moonlight Chrome](https://github.com/moonlight-stream/moonlight-chrome) for streaming on PC.

Moonlight also has mobile versions for [Android](https://github.com/moonlight-stream/moonlight-android) and  [iOS](https://github.com/moonlight-stream/moonlight-ios).

This client is currently still in development (pre-alpha), but the streaming performance is already much better than the older Moonlight Chrome client on many machines. Test releases may be available for your OS on the [Releases page](https://github.com/moonlight-stream/moonlight-qt/releases).

You can follow development on our [Discord server](https://discord.gg/6ERtzFY).

 ![Windows AppVeyor Status](https://ci.appveyor.com/api/projects/status/glj5cxqwy2w3bglv/branch/master?svg=true)

## Features
 - Hardware accelerated video decoding on Windows, Mac, and Linux
 - Supports streaming at up to 90 FPS on high refresh rate monitors
 - Supports streaming at 720p, 1080p, 1440p, or 4K
 - 5.1 surround sound audio
 - HEVC support for better image quality at reduced bandwidth
 - Keyboard and mouse support
 - Gamepad support with SDL gamepad mappings

## Building
1. Install the latest Qt SDK (and optionally, the Qt Creator IDE) from https://www.qt.io/download (select MSVC toolchain on Windows)
2. Run `git submodule update --init --recursive` from within `moonlight-qt/`
3. Open the project in Qt Creator or build from qmake on the command line

## Contribute
1. Fork us
2. Write code
3. Send Pull Requests

Check out our [website](http://moonlight-stream.com) for project links and information.
