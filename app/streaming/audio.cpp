#include "session.hpp"

#include <Limelight.h>
#include <SDL.h>

#define MAX_CHANNELS 6
#define SAMPLES_PER_FRAME 240
#define MIN_QUEUED_FRAMES 2
#define MAX_QUEUED_FRAMES 4
#define STOP_THE_WORLD_LIMIT 20
#define DROP_RATIO_DENOM 32

SDL_AudioDeviceID Session::s_AudioDevice;
OpusMSDecoder* Session::s_OpusDecoder;
short Session::s_OpusDecodeBuffer[MAX_CHANNELS * SAMPLES_PER_FRAME];
int Session::s_ChannelCount;
int Session::s_PendingDrops;
int Session::s_PendingHardDrops;
unsigned int Session::s_SampleIndex;
Uint32 Session::s_BaselinePendingData;

int Session::sdlDetermineAudioConfiguration()
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
    int ret;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        return AUDIO_CONFIGURATION_STEREO;
    }

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
        ret = AUDIO_CONFIGURATION_STEREO;
        goto Exit;
    }

    SDL_CloseAudioDevice(dev);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio device has %d channels", have.channels);

    if (have.channels > 2) {
        // We don't support quadraphonic or 7.1 surround, but SDL
        // should be able to downmix or upmix better from 5.1 than
        // from stereo, so use 5.1 in non-stereo cases.
        ret = AUDIO_CONFIGURATION_51_SURROUND;
    }
    else {
        ret = AUDIO_CONFIGURATION_STEREO;
    }

Exit:
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return ret;
}

bool Session::testAudio(int audioConfiguration)
{
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
    bool ret;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Audio test - SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_zero(want);
    want.freq = 48000;
    want.format = AUDIO_S16;
    want.samples = SAMPLES_PER_FRAME;

    switch (audioConfiguration) {
    case AUDIO_CONFIGURATION_STEREO:
        want.channels = 2;
        break;
    case AUDIO_CONFIGURATION_51_SURROUND:
        want.channels = 6;
        break;
    default:
        SDL_assert(false);
        ret = false;
        goto Exit;
    }

    // Test audio device for functionality
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Audio test - Failed to open audio device: %s",
                     SDL_GetError());
        ret = false;
        goto Exit;
    }

    SDL_CloseAudioDevice(dev);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio test - Successful with %d channels",
                want.channels);

    ret = true;

Exit:
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return ret;
}

int Session::sdlAudioInit(int /* audioConfiguration */,
                          POPUS_MULTISTREAM_CONFIGURATION opusConfig,
                          void* /* arContext */, int /* arFlags */)
{
    SDL_AudioSpec want, have;
    int error;

    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                     SDL_GetError());
        return -1;
    }

    SDL_zero(want);
    want.freq = opusConfig->sampleRate;
    want.format = AUDIO_S16;
    want.channels = opusConfig->channelCount;

    // This is supposed to be a power of 2, but our
    // frames contain a non-power of 2 number of samples,
    // so the slop would require buffering another full frame.
    // Specifying non-Po2 seems to work for our supported platforms.
    want.samples = SAMPLES_PER_FRAME;

    s_AudioDevice = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_AudioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to open audio device: %s",
                     SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
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
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return -2;
    }

    // SDL counts pending samples in the queued
    // audio size using the WASAPI backend. This
    // includes silence, which can throw off our
    // pending data count. Get a baseline so we
    // can exclude that data.
    s_BaselinePendingData = 0;
#ifdef Q_OS_WIN32
    for (int i = 0; i < 100; i++) {
        s_BaselinePendingData = qMax(s_BaselinePendingData, SDL_GetQueuedAudioSize(s_AudioDevice));
        SDL_Delay(10);
    }
#endif
    s_BaselinePendingData *= 2;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Baseline pending audio data: %d bytes",
                s_BaselinePendingData);

    s_ChannelCount = opusConfig->channelCount;
    s_SampleIndex = 0;
    s_PendingDrops = s_PendingHardDrops = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio stream has %d channels",
                opusConfig->channelCount);

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
    s_OpusDecoder = nullptr;

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_assert(!SDL_WasInit(SDL_INIT_AUDIO));
}

void Session::sdlAudioDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int samplesDecoded;

    s_SampleIndex++;

    Uint32 queuedAudio = qMax((int)SDL_GetQueuedAudioSize(s_AudioDevice) - (int)s_BaselinePendingData, 0);
    Uint32 framesQueued = queuedAudio / (SAMPLES_PER_FRAME * s_ChannelCount * sizeof(short));

    // We must check this prior to the below checks to ensure we don't
    // underflow if framesQueued - s_PendingHardDrops < 0.
    if (framesQueued <= MIN_QUEUED_FRAMES) {
        s_PendingDrops = s_PendingHardDrops = 0;
    }
    // Pend enough drops to get us back to MIN_QUEUED_FRAMES
    else if (framesQueued - s_PendingHardDrops > STOP_THE_WORLD_LIMIT) {
        s_PendingHardDrops = framesQueued - MIN_QUEUED_FRAMES;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Pending hard drop of %u audio frames",
                    s_PendingHardDrops);
    }
    else if (framesQueued - s_PendingHardDrops - s_PendingDrops > MAX_QUEUED_FRAMES) {
        s_PendingDrops = framesQueued - MIN_QUEUED_FRAMES;
    }

    // Determine if this frame should be dropped
    if (s_PendingHardDrops != 0) {
        // Hard drops happen all at once to forcefully
        // resync with the source.
        s_PendingHardDrops--;
        return;
    }
    else if (s_PendingDrops != 0 && s_SampleIndex % DROP_RATIO_DENOM == 0) {
        // Normal drops are interspersed with the audio data
        // to hide the glitches.
        s_PendingDrops--;
        return;
    }

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
