#include "soundioaudiorenderer.h"

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <QtGlobal>

SoundIoAudioRenderer::SoundIoAudioRenderer()
    : m_OpusChannelCount(0),
      m_SoundIo(nullptr),
      m_Device(nullptr),
      m_OutputStream(nullptr),
      m_RingBuffer(nullptr),
      m_AudioPacketDuration(0),
      m_Latency(0),
      m_Errored(false)
{

}

SoundIoAudioRenderer::~SoundIoAudioRenderer()
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio latency: %f",
                m_Latency);

    if (m_OutputStream != nullptr) {
        soundio_outstream_destroy(m_OutputStream);
    }

    // Must be destroyed after the stream is stopped
    // or we could still get sioWriteCallback() calls.
    if (m_RingBuffer != nullptr) {
        soundio_ring_buffer_destroy(m_RingBuffer);
    }

    if (m_Device != nullptr) {
        soundio_device_unref(m_Device);
    }

    if (m_SoundIo != nullptr) {
        soundio_destroy(m_SoundIo);
    }
}

int SoundIoAudioRenderer::scoreChannelLayout(const struct SoundIoChannelLayout* layout, const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    int score = 50;

    // Compute a score for this layout based on how many matching channels
    // we find (or acceptable alternatives).
    for (int i = 0; i < layout->channel_count; i++) {
        if (opusConfig->channelCount >= 2) {
            switch (layout->channels[i]) {
            case SoundIoChannelIdFrontLeft:
            case SoundIoChannelIdFrontRight:
                score += 2;
                break;
            default:
                break;
            }
        }

        if (opusConfig->channelCount >= 6) {
            switch (layout->channels[i]) {
            case SoundIoChannelIdFrontCenter:
            case SoundIoChannelIdLfe:
                score += 2;
                break;

            case SoundIoChannelIdSideLeft:
            case SoundIoChannelIdSideRight:
                score++;
                break;

            case SoundIoChannelIdBackLeft:
            case SoundIoChannelIdBackRight:
                if (opusConfig->channelCount == 6) {
                    // Score back channels higher in 5.1 mode to
                    // discourage selection of side channel layouts.
                    score += 2;
                }
                else {
                    // Score back channels normally for 7.1 mode
                    score++;
                }
                break;

            default:
                break;
            }
        }
    }

    // Now subtract the difference between the desired and actual channel count
    // to punish layouts that have extra unused speakers.
    if (opusConfig->channelCount < layout->channel_count) {
        score -= layout->channel_count - opusConfig->channelCount;
    }

    return score;
}

bool SoundIoAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    m_SoundIo = soundio_create();
    if (m_SoundIo == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_create() failed");
        return false;
    }

    m_SoundIo->app_name = "Moonlight";
    m_SoundIo->userdata = this;
    m_SoundIo->on_backend_disconnect = sioBackendDisconnect;
    m_SoundIo->on_devices_change = sioDevicesChanged;

    int err = soundio_connect(m_SoundIo);
    if (err != SoundIoErrorNone) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_connect() failed: %s",
                     soundio_strerror(err));
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio backend: %s",
                soundio_backend_name(m_SoundIo->current_backend));

    // Don't continue if we could only open the dummy backend
    if (m_SoundIo->current_backend == SoundIoBackendDummy) {
        return false;
    }

    // Flush events to update with new device arrivals
    soundio_flush_events(m_SoundIo);

    // Remember the actual channel count for later
    m_OpusChannelCount = opusConfig->channelCount;

    int outputDeviceIndex = soundio_default_output_device_index(m_SoundIo);
    if (outputDeviceIndex < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No output device found");
        return false;
    }

    m_Device = soundio_get_output_device(m_SoundIo, outputDeviceIndex);
    if (m_Device == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_get_output_device() failed");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Selected audio device: %s",
                m_Device->name);

    m_OutputStream = soundio_outstream_create(m_Device);
    if (m_OutputStream == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_outstream_create() failed");
        return false;
    }

    m_AudioPacketDuration = (opusConfig->samplesPerFrame / (opusConfig->sampleRate / 1000)) / 1000.0;

    m_OutputStream->format = SoundIoFormatS16NE;
    m_OutputStream->sample_rate = opusConfig->sampleRate;
    m_OutputStream->software_latency = m_AudioPacketDuration;
    m_OutputStream->name = "Moonlight";
    m_OutputStream->userdata = this;
    m_OutputStream->error_callback = sioErrorCallback;
    m_OutputStream->write_callback = sioWriteCallback;

    SoundIoChannelLayout bestLayout = m_Device->current_layout;
    for (int i = 0; i < m_Device->layout_count; i++) {
        if (scoreChannelLayout(&bestLayout, opusConfig) <
                scoreChannelLayout(&m_Device->layouts[i], opusConfig)) {
            bestLayout = m_Device->layouts[i];
        }
    }

    if (bestLayout.channel_count < opusConfig->channelCount) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No compatible channel layouts found. Some channels may not be played!");
    }

    m_OutputStream->layout = bestLayout;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Native layout: %s (%d channels)",
                m_OutputStream->layout.name ?
                    m_OutputStream->layout.name : "<UNKNOWN>",
                m_OutputStream->layout.channel_count);

    err = soundio_outstream_open(m_OutputStream);
    if (err != SoundIoErrorNone) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_outstream_open() failed: %s",
                     soundio_strerror(err));
        return false;
    }

    if (m_OutputStream->layout_error) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Channel layout failed: %s",
                    soundio_strerror(m_OutputStream->layout_error));

        // ALSA through PulseAudio appears to fail snd_pcm_set_chmap()
        // even after claiming the layout is supported (and even on totally
        // standard layouts like Stereo). We'll just ignore this for ALSA
        // and only bail if we get an actual failure out of one of these APIs.
        if (m_SoundIo->current_backend != SoundIoBackendAlsa) {
            return false;
        }
    }

    m_EffectiveLayout = m_OutputStream->layout;
    for (int i = 0; i < m_EffectiveLayout.channel_count; i++) {
        if (opusConfig->channelCount == 6) {
            // For 5.1, replace side L/R with back L/R so our channel position
            // logic in sioWriteCallback() works.
            if (m_EffectiveLayout.channels[i] == SoundIoChannelIdSideLeft) {
                m_EffectiveLayout.channels[i] = SoundIoChannelIdBackLeft;
            }
            if (m_EffectiveLayout.channels[i] == SoundIoChannelIdSideRight) {
                m_EffectiveLayout.channels[i] = SoundIoChannelIdBackRight;
            }
        }
        else if (opusConfig->channelCount == 8) {
            // For 5.1, replace side L/R with LOC/ROC so our channel position
            // logic in sioWriteCallback() works.
            if (m_EffectiveLayout.channels[i] == SoundIoChannelIdSideLeft) {
                m_EffectiveLayout.channels[i] = SoundIoChannelIdFrontLeftCenter;
            }
            if (m_EffectiveLayout.channels[i] == SoundIoChannelIdSideRight) {
                m_EffectiveLayout.channels[i] = SoundIoChannelIdFrontRightCenter;
            }
        }
    }

    int packetsToBuffer;

    if (m_SoundIo->current_backend == SoundIoBackendWasapi) {
        // 15 ms buffer seems to be fine for WASAPI
        packetsToBuffer = (int)ceil(0.015 / m_AudioPacketDuration);
    }
    else {
        // 30 ms buffer on CoreAudio to avoid glitching on macOS
        packetsToBuffer = (int)ceil(0.030 / m_AudioPacketDuration);
    }

    // Always buffer at least 2 packets
    packetsToBuffer = qMax(2, packetsToBuffer);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Audio buffer size: %f seconds",
                packetsToBuffer * m_AudioPacketDuration);

    m_RingBuffer = soundio_ring_buffer_create(m_SoundIo,
                                              m_OutputStream->bytes_per_sample *
                                              m_OpusChannelCount *
                                              opusConfig->samplesPerFrame *
                                              packetsToBuffer);
    if (m_RingBuffer == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_ring_buffer_create() failed");
        return false;
    }

    err = soundio_outstream_start(m_OutputStream);
    if (err != SoundIoErrorNone) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "soundio_outstream_start() failed: %s",
                     soundio_strerror(err));
        return false;
    }

    // HACK: For some reason, a constant latency hangs around in the audio pipeline
    // unless we wait for the audio stream to drain before actually submitting any samples.
    // This is a gross hack, but it works remarkably well.
    SDL_Delay(500);

    return true;
}

void* SoundIoAudioRenderer::getAudioBuffer(int* size)
{
    // We must always write a full frame of audio. If we don't,
    // the reader will get out of sync with the writer and our
    // channels will get all mixed up. To ensure this is always
    // the case, round our bytes free down to the next multiple
    // of our frame size.
    int bytesFree = soundio_ring_buffer_free_count(m_RingBuffer);
    int bytesPerFrame = m_OpusChannelCount * m_OutputStream->bytes_per_sample;
    *size = qMin(*size, (bytesFree / bytesPerFrame) * bytesPerFrame);
    return soundio_ring_buffer_write_ptr(m_RingBuffer);
}

bool SoundIoAudioRenderer::submitAudio(int bytesWritten)
{
    if (m_Errored) {
        return false;
    }

    if (bytesWritten == 0) {
        // Nothing to do
        return true;
    }

    // Flush events to update with new device arrivals
    soundio_flush_events(m_SoundIo);

    // Advance the write pointer
    soundio_ring_buffer_advance_write_ptr(m_RingBuffer, bytesWritten);

    return true;
}

int SoundIoAudioRenderer::getCapabilities()
{
    // TODO: Tweak buffer sizes then re-enable arbitrary audio duration
    return CAPABILITY_DIRECT_SUBMIT /* | CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION */;
}

void SoundIoAudioRenderer::sioErrorCallback(SoundIoOutStream* stream, int err)
{
    auto me = reinterpret_cast<SoundIoAudioRenderer*>(stream->userdata);

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Audio rendering error: %s",
                 soundio_strerror(err));

    // Trigger reinitialization
    me->m_Errored = true;
}

void SoundIoAudioRenderer::sioBackendDisconnect(SoundIo* soundio, int err)
{
    auto me = reinterpret_cast<SoundIoAudioRenderer*>(soundio->userdata);

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Audio backend disconnected: %s",
                soundio_strerror(err));

    // Trigger reinitialization
    me->m_Errored = true;
}

void SoundIoAudioRenderer::sioDevicesChanged(SoundIo* soundio)
{
    auto me = reinterpret_cast<SoundIoAudioRenderer*>(soundio->userdata);

    if (me->m_Device == nullptr) {
        // Ignore calls that take place during initialization
        return;
    }

    int outputDeviceIndex = soundio_default_output_device_index(soundio);
    if (outputDeviceIndex >= 0) {
        struct SoundIoDevice* outputDevice = soundio_get_output_device(soundio, outputDeviceIndex);
        if (outputDevice == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "soundio_get_output_device() failed");
            return;
        }

        if (!soundio_device_equal(outputDevice, me->m_Device)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Default audio output device changed");

            // Trigger reinitialization
            me->m_Errored = true;
        }

        soundio_device_unref(outputDevice);
    }
}

// bytes_per_frame should never be used on the ring buffer! It's not always
// the same number of bytes per frames as the output stream!
void SoundIoAudioRenderer::sioWriteCallback(SoundIoOutStream* stream, int frameCountMin, int frameCountMax)
{
    auto me = reinterpret_cast<SoundIoAudioRenderer*>(stream->userdata);
    char* readPtr = soundio_ring_buffer_read_ptr(me->m_RingBuffer);
    int framesLeft = soundio_ring_buffer_fill_count(me->m_RingBuffer) /
            (me->m_OpusChannelCount * stream->bytes_per_sample);
    int bytesRead = 0;

    // Ensure we always write at least a buffer, even if it's silence, to avoid
    // busy looping when no audio data is available while libsoundio tries to keep
    // us from starving the output device.
    frameCountMin = qMax(frameCountMin, (int)(stream->sample_rate * me->m_AudioPacketDuration));

    // Clamp frameCountMax to at least 2 packets or 20 ms to stop our latency from growing if audio packets lag.
    // This makes sure that we never increase our latency far beyond what the sink is consuming.
    frameCountMax = qMin(frameCountMax, (int)(stream->sample_rate * qMax(me->m_AudioPacketDuration * 2, 0.020)));
    frameCountMin = qMin(frameCountMin, frameCountMax);

    // Clamp framesLeft to frameCountMax
    framesLeft = qMin(framesLeft, frameCountMax);

    // Track latency on queueing-based backends
    if (me->m_SoundIo->current_backend != SoundIoBackendCoreAudio && me->m_SoundIo->current_backend != SoundIoBackendJack) {
        soundio_outstream_get_latency(stream, &me->m_Latency);
    }

    for (;;) {
        int frameCount;
        int err;
        struct SoundIoChannelArea* areas;

        // Always meet the minimum but don't write more than that
        // if we'll have to insert silence
        frameCount = qMax(framesLeft, frameCountMin);

        if (frameCount == 0) {
            // Nothing more to write
            break;
        }

        err = soundio_outstream_begin_write(stream, &areas, &frameCount);
        if (err != SoundIoErrorNone) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "soundio_outstream_begin_write() failed: %s",
                         soundio_strerror(err));
            break;
        }

        for (int frame = 0; frame < frameCount; frame++) {
            for (int ch = 0; ch < me->m_EffectiveLayout.channel_count; ch++) {
                // SoundIoChannelId - 1 happens to match Moonlight's channel layout
                // after we've applied our fixups to m_EffectiveLayout for 5.1 and 7.1.
                int readPtrChannel = me->m_EffectiveLayout.channels[ch] - 1;

                if (frame >= framesLeft || readPtrChannel >= me->m_OpusChannelCount) {
                    // Write silence if we have no buffered frames left or
                    // nothing in the audio stream for this channel
                    memset(areas[ch].ptr, 0, stream->bytes_per_sample);
                }
                else {
                    // Write audio data from our ring buffer
                    memcpy(areas[ch].ptr,
                           &readPtr[readPtrChannel * stream->bytes_per_sample],
                           stream->bytes_per_sample);
                }

                areas[ch].ptr += areas[ch].step;
            }

            // Move on to the next frame if we aren't inserting silence
            if (frame < framesLeft) {
                readPtr += stream->bytes_per_sample * me->m_OpusChannelCount;
                bytesRead += stream->bytes_per_sample * me->m_OpusChannelCount;
            }
        }

        err = soundio_outstream_end_write(stream);
        if (err != SoundIoErrorNone && err != SoundIoErrorUnderflow) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "soundio_outstream_end_write() failed: %s",
                         soundio_strerror(err));
            break;
        }

        if (framesLeft >= frameCount) {
            framesLeft -= frameCount;
        }
        else {
            framesLeft = 0;
        }

        if (frameCountMin >= frameCount) {
            frameCountMin -= frameCount;
        }
        else {
            frameCountMin = 0;
        }
    }

    soundio_ring_buffer_advance_read_ptr(me->m_RingBuffer, bytesRead);
}
