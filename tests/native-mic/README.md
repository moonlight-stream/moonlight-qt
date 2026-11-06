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
