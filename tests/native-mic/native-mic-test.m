// This harness links the actual capture/packet implementation. No input device
// is opened: only synthetic tone packets and a malformed callback buffer.
#include "../../app/streaming/native-mic.m"
int testInvalidCaptureBuffer(void) {
    Sender fixture = {.fd = -1};
    AudioQueueBuffer buffer = {.mAudioDataBytesCapacity = 2, .mAudioDataByteSize = 3};
    atomic_store(&capturing, true);
    atomic_store(&captureFailed, false);
    captured(&fixture, NULL, &buffer, NULL, 0, NULL);
    int result = nativeMicStatus();
    stopNativeMic();
    return result;
}
