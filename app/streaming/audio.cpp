#include "session.hpp"

#include <Limelight.h>
#include <SDL.h>

#define MAX_CHANNELS 6
#define SAMPLES_PER_FRAME 240

SDL_AudioDeviceID Session::s_AudioDevice;
OpusMSDecoder* Session::s_OpusDecoder;
short Session::s_OpusDecodeBuffer[MAX_CHANNELS * SAMPLES_PER_FRAME];
int Session::s_ChannelCount;

int Session::sdlDetermineAudioConfiguration()
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.channels = 6;
    want.samples = 1024;

    // Try to open for 5.1 surround sound, but allow SDL to tell us that's
    // not available.
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device");
        // We'll probably have issues during audio stream init, but we'll
        // try anyway
        return AUDIO_CONFIGURATION_STEREO;
    }

    SDL_CloseAudioDevice(dev);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio device has %d channels", have.channels);

    if (have.channels > 2) {
        // We don't support quadraphonic or 7.1 surround, but SDL
        // should be able to downmix or upmix better from 5.1 than
        // from stereo, so use 5.1 in non-stereo cases.
        return AUDIO_CONFIGURATION_51_SURROUND;
    }
    else {
        return AUDIO_CONFIGURATION_STEREO;
    }
}

int Session::sdlAudioInit(int audioConfiguration,
                          POPUS_MULTISTREAM_CONFIGURATION opusConfig,
                          void* arContext, int arFlags)
{
    SDL_AudioSpec want, have;
    int error;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_S16;
    want.channels = opusConfig->channelCount;
    want.samples = 1024;

    s_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return -1;
    }

    s_OpusDecoder = opus_multistream_decoder_create(opusConfig->sampleRate,
                                                    opusConfig->channelCount,
                                                    opusConfig->streams,
                                                    opusConfig->coupledStreams,
                                                    opusConfig->mapping,
                                                    &error);
    if (s_OpusDecoder == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create decoder: %d",
                     error);
        SDL_CloseAudioDevice(s_AudioDevice);
        s_AudioDevice = 0;
        return -2;
    }

    s_ChannelCount = opusConfig->channelCount;

    return 0;
}

void Session::sdlAudioStart()
{
    // Unpause the audio device
    SDL_PauseAudioDevice(s_AudioDevice, 0);
}

void Session::sdlAudioStop()
{
    // Pause the audio device
    SDL_PauseAudioDevice(s_AudioDevice, 1);
}

void Session::sdlAudioCleanup()
{
    SDL_CloseAudioDevice(s_AudioDevice);
    s_AudioDevice = 0;

    opus_multistream_decoder_destroy(s_OpusDecoder);
    s_OpusDecoder = NULL;
}

void Session::sdlAudioDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int samplesDecoded;

    samplesDecoded = opus_multistream_decode(s_OpusDecoder,
                                             (unsigned char*)sampleData,
                                             sampleLength,
                                             s_OpusDecodeBuffer,
                                             SAMPLES_PER_FRAME,
                                             0);
    if (samplesDecoded > 0) {
        if (SDL_QueueAudio(s_AudioDevice,
                           s_OpusDecodeBuffer,
                           sizeof(short) * samplesDecoded * s_ChannelCount) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to queue audio sample: %s",
                         SDL_GetError());
        }
    }
}
