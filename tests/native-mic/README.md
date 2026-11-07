# Native microphone sender checks

These macOS tests link the actual sender source and send synthetic audio to loopback. They never call startNativeMic or open a recording device. Requirements: Apple Clang, the normal Moonlight macOS dependency headers, libopus, and Python with the cryptography package. Set OPUS_LIBRARY if libopus is outside the standard lookup path.

From the repository root, build the test library (adjust the libopus library directory for your dependency installation):

```sh
clang -dynamiclib -fblocks -Wall -Wextra -Werror -Wno-unused-parameter -I libs/mac/include -L /opt/homebrew/opt/opus/lib -lopus -framework Cocoa -framework AVFoundation -framework AudioToolbox tests/native-mic/native-mic-test.m -o tests/native-mic/libnative-mic-test.dylib
python3 tests/native-mic/check_sender.py
```

The fixture validates private-address/plaintext rejection, malformed callback stopping, occupied source-port handling, 300 encrypted RTP/Opus packets and their timestamps, periodic PING, a decoded 1 kHz tone, pacing, ten start/stop cycles and socket release. It is not proof of Windows endpoint delivery, macOS permission UI behavior, or long-session cryptographic security. Source port 50002 must be free before running this fixture; an occupied port produces a test failure rather than stopping another process.

## Status composition contract

The overlay fixture compiles the production overlay manager without opening a window or input device:

```sh
cd tests/native-mic/overlay
qmake overlay-test.pro
make
DYLD_LIBRARY_PATH=../../../libs/mac/lib ./overlay-test
```

It checks that microphone status and connection/gamepad warnings compose correctly, clearing either part preserves the other, and the debug channel remains independent.

### Shared sender and SDL capture

Run after building the Qt release target on macOS. The fixture replaces the
negotiated session inputs with loopback values. It uses the actual SDL capture
adapter, Opus, RTP sender and OpenSSL encryption with SDL's dummy audio driver.
Python requires `cryptography`. No microphone hardware is opened.

```sh
common=moonlight-common-c/moonlight-common-c
fixture=tests/native-mic/portable
objects=moonlight-common-c/release
cc -c "$fixture/fixture.c" -o "$fixture/fixture.o" \
    -I"$common/src" -I"$common/enet/include" -Ilibs/mac/include
c++ -std=c++17 -dynamiclib app/streaming/native-mic-sdl.cpp \
    "$fixture/fixture.o" "$objects/NativeMicStream.o" "$objects/PlatformCrypto.o" \
    -I"$common/src" -Ilibs/mac/include/SDL2 -Ilibs/mac/include \
    -Llibs/mac/lib -lSDL2 -lopus.0 -lcrypto.3 \
    -o "$fixture/libportable-mic-test.dylib"
install_name_tool -change @loader_path/../Frameworks/libcrypto.3.dylib \
    "$PWD/libs/mac/lib/libcrypto.3.dylib" "$fixture/libportable-mic-test.dylib"
codesign --force --sign - "$fixture/libportable-mic-test.dylib"
DYLD_LIBRARY_PATH="$PWD/libs/mac/lib" python3 "$fixture/check.py"
```

This checks capture callback framing, endpoint/opt-in gates, encryption and
decoding, occupied-port behavior, and teardown/restart. Windows and Linux device
capture and live native host delivery require their own acceptance tests.
