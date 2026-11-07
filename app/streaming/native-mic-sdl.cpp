#include "native-mic.h"

#include <Limelight.h>
#include <SDL.h>
#include <opus.h>

#include <atomic>
#include <cstdint>
#include <cstring>

namespace {

constexpr int FrameSamples = 240;
constexpr int EncodedBytes = 20;

struct Capture {
    SDL_AudioDeviceID device = 0;
    bool audioInitialized = false;
    NATIVE_MIC_SENDER* sender = nullptr;
    OpusEncoder* encoder = nullptr;
    opus_int16 frame[FrameSamples];
    int samples = 0;
    std::atomic<int> status{0};
};

Capture capture;

void SDLCALL receiveAudio(void* context, Uint8* bytes, int length)
{
    auto& state = *static_cast<Capture*>(context);
    if (state.status.load() != 1) {
        return;
    }
    if (length < 0 || length % sizeof(opus_int16) != 0) {
        state.status.store(-1);
        return;
    }

    while (length > 0) {
        int copySamples = SDL_min(length / int(sizeof(opus_int16)), FrameSamples - state.samples);
        int copyBytes = copySamples * sizeof(opus_int16);
        std::memcpy(state.frame + state.samples, bytes, copyBytes);
        state.samples += copySamples;
        bytes += copyBytes;
        length -= copyBytes;
        if (state.samples != FrameSamples) {
            continue;
        }
        unsigned char encoded[EncodedBytes];
        if (opus_encode(state.encoder, state.frame, FrameSamples, encoded, sizeof(encoded)) != EncodedBytes ||
                LiSendNativeMicFrame(state.sender, encoded, sizeof(encoded)) != 0) {
            state.status.store(-1);
            return;
        }
        state.samples = 0;
    }
}

} // namespace

void stopNativeMic()
{
    capture.status.store(0);
    // SDL waits for the callback to finish before closing the device.
    if (capture.device != 0) {
        SDL_CloseAudioDevice(capture.device);
        capture.device = 0;
    }
    LiDestroyNativeMicSender(capture.sender);
    capture.sender = nullptr;
    if (capture.encoder != nullptr) {
        opus_encoder_destroy(capture.encoder);
        capture.encoder = nullptr;
    }
    if (capture.audioInitialized) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        capture.audioInitialized = false;
    }
    capture.samples = 0;
    capture.status.store(0);
}

int nativeMicStatus()
{
    int status = capture.status.load();
    if (status == 1 && SDL_GetAudioDeviceStatus(capture.device) != SDL_AUDIO_PLAYING) {
        return -1;
    }
    return status;
}

int startNativeMic(const char*, unsigned short, const char*, const char*)
{
    stopNativeMic();
    capture.sender = LiCreateNativeMicSender();
    if (capture.sender == nullptr) {
        return 3;
    }

    int error;
    capture.encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &error);
    if (error != OPUS_OK || capture.encoder == nullptr ||
            opus_encoder_ctl(capture.encoder, OPUS_SET_BITRATE(32000)) != OPUS_OK ||
            opus_encoder_ctl(capture.encoder, OPUS_SET_VBR(0)) != OPUS_OK ||
            opus_encoder_ctl(capture.encoder, OPUS_SET_DTX(0)) != OPUS_OK) {
        stopNativeMic();
        return 4;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Microphone audio initialization failed: %s", SDL_GetError());
        stopNativeMic();
        return 6;
    }
    capture.audioInitialized = true;

    SDL_AudioSpec requested{};
    requested.freq = 48000;
    requested.format = AUDIO_S16SYS;
    requested.channels = 1;
    requested.samples = 256;
    requested.callback = receiveAudio;
    requested.userdata = &capture;
    capture.device = SDL_OpenAudioDevice(nullptr, 1, &requested, nullptr, 0);
    if (capture.device == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Default microphone could not open: %s", SDL_GetError());
        stopNativeMic();
        return 6;
    }

    capture.status.store(1);
    SDL_PauseAudioDevice(capture.device, 0);
    return 0;
}
