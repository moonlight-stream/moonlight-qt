#include <Limelight.h>
#include <opus_multistream.h>
#include <SDL.h>

#define MAX_CHANNELS 6
#define SAMPLES_PER_FRAME 240

static SDL_AudioDeviceID g_AudioDevice;
static OpusMSDecoder* g_OpusDecoder;
static short g_OpusDecodeBuffer[MAX_CHANNELS * SAMPLES_PER_FRAME];
static OPUS_MULTISTREAM_CONFIGURATION g_OpusConfig;

int SdlDetermineAudioConfiguration(void)
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

int SdlAudioInit(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* arContext, int arFlags)
{
    SDL_AudioSpec want, have;
    int error;

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_S16;
    want.channels = opusConfig->channelCount;
    want.samples = 1024;

    g_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (g_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        return -1;
    }

    g_OpusDecoder = opus_multistream_decoder_create(opusConfig->sampleRate,
                                                    opusConfig->channelCount,
                                                    opusConfig->streams,
                                                    opusConfig->coupledStreams,
                                                    opusConfig->mapping,
                                                    &error);
    if (g_OpusDecoder == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create decoder: %d",
                     error);
        SDL_CloseAudioDevice(g_AudioDevice);
        g_AudioDevice = 0;
        return -2;
    }

    SDL_memcpy(&g_OpusConfig, opusConfig, sizeof(g_OpusConfig));

    return 0;
}

void SdlAudioStart(void)
{
    // Unpause the audio device
    SDL_PauseAudioDevice(g_AudioDevice, 0);
}

void SdlAudioStop(void)
{
    // Pause the audio device
    SDL_PauseAudioDevice(g_AudioDevice, 1);
}

void SdlAudioCleanup(void)
{
    SDL_CloseAudioDevice(g_AudioDevice);
    g_AudioDevice = 0;

    opus_multistream_decoder_destroy(g_OpusDecoder);
    g_OpusDecoder = NULL;
}

void SdlAudioDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int samplesDecoded;

    samplesDecoded = opus_multistream_decode(g_OpusDecoder,
                                             (unsigned char*)sampleData,
                                             sampleLength,
                                             g_OpusDecodeBuffer,
                                             SAMPLES_PER_FRAME,
                                             0);
    if (samplesDecoded > 0) {
        if (SDL_QueueAudio(g_AudioDevice,
                           g_OpusDecodeBuffer,
                           sizeof(short) * samplesDecoded * g_OpusConfig.channelCount) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to queue audio sample: %s",
                         SDL_GetError());
        }
    }
}
