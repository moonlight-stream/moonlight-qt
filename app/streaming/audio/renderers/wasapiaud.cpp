#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER

#include "wasapi.h"

#include "SDL_compat.h"

#include <Limelight.h>

#include "miniaudio/miniaudio.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr int QUEUE_CAPACITY_MS = 100;
constexpr int MAX_BACKPRESSURE_WAIT_MS = 100;
constexpr ma_uint32 DEVICE_PERIOD_MS = 10;
constexpr ma_uint32 DEVICE_PERIODS = 4;

bool isSupportedChannelCount(int channelCount)
{
    return channelCount == 1 || channelCount == 2 || channelCount == 6 || channelCount == 8;
}

}

class WasapiAudioRenderer::Impl
{
public:
    ~Impl()
    {
        stop();
    }

    bool prepare(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
    {
        if (opusConfig == nullptr || m_DeviceInitialized) {
            return false;
        }

        if (opusConfig->sampleRate <= 0 || opusConfig->samplesPerFrame <= 0 ||
                !isSupportedChannelCount(opusConfig->channelCount)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unsupported WASAPI stream layout: %d Hz, %d channels",
                         opusConfig->sampleRate,
                         opusConfig->channelCount);
            return false;
        }

        m_Config = *opusConfig;
        m_DecodeBuffer.resize(static_cast<size_t>(m_Config.samplesPerFrame) *
                              m_Config.channelCount);
        ma_channel_map_init_standard(ma_standard_channel_map_microsoft,
                                      m_ChannelMap,
                                      MA_MAX_CHANNELS,
                                      static_cast<ma_uint32>(m_Config.channelCount));

        ma_backend backends[] = {ma_backend_wasapi};
        ma_result result = ma_context_init(backends, 1, nullptr, &m_Context);
        if (result != MA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to initialize miniaudio WASAPI context: %d",
                         result);
            return false;
        }
        m_ContextInitialized = true;

        const ma_uint32 queueCapacity = std::max(
            static_cast<ma_uint32>(m_Config.sampleRate * QUEUE_CAPACITY_MS / 1000),
            static_cast<ma_uint32>(m_Config.samplesPerFrame * DEVICE_PERIODS));
        result = ma_pcm_rb_init(ma_format_f32,
                                static_cast<ma_uint32>(m_Config.channelCount),
                                queueCapacity,
                                nullptr,
                                nullptr,
                                &m_RingBuffer);
        if (result != MA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to initialize miniaudio audio queue: %d",
                         result);
            stop();
            return false;
        }
        m_RingBufferInitialized = true;

        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.sampleRate = static_cast<ma_uint32>(m_Config.sampleRate);
        deviceConfig.periodSizeInMilliseconds = DEVICE_PERIOD_MS;
        deviceConfig.periods = DEVICE_PERIODS;
        deviceConfig.dataCallback = dataCallback;
        deviceConfig.notificationCallback = notificationCallback;
        deviceConfig.pUserData = this;
        deviceConfig.wasapi.usage = ma_wasapi_usage_pro_audio;
        deviceConfig.playback.format = ma_format_f32;
        deviceConfig.playback.channels = static_cast<ma_uint32>(m_Config.channelCount);
        deviceConfig.playback.pChannelMap = m_ChannelMap;
        deviceConfig.playback.shareMode = ma_share_mode_exclusive;

        result = ma_device_init(&m_Context, &deviceConfig, &m_Device);
        if (result != MA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to initialize exclusive WASAPI device: %d",
                         result);
            stop();
            return false;
        }
        m_DeviceInitialized = true;

        result = ma_device_start(&m_Device);
        if (result != MA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unable to start exclusive WASAPI device: %d",
                         result);
            stop();
            return false;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "WASAPI exclusive audio: %d Hz, %d channels",
                    m_Config.sampleRate,
                    m_Config.channelCount);
        return true;
    }

    void* getBuffer(int* size)
    {
        if (m_DecodeBuffer.empty()) {
            return nullptr;
        }

        if (size != nullptr) {
            *size = std::min(*size, static_cast<int>(m_DecodeBuffer.size() * sizeof(float)));
        }
        return m_DecodeBuffer.data();
    }

    bool submit(int bytesWritten)
    {
        if (m_FatalError.load(std::memory_order_acquire)) {
            return false;
        }

        if (bytesWritten == 0) {
            return true;
        }

        const size_t bufferBytes = m_DecodeBuffer.size() * sizeof(float);
        const size_t frameBytes = static_cast<size_t>(m_Config.channelCount) * sizeof(float);
        if (bytesWritten < 0 || static_cast<size_t>(bytesWritten) > bufferBytes ||
                static_cast<size_t>(bytesWritten) % frameBytes != 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Invalid WASAPI audio submission size: %d bytes",
                         bytesWritten);
            return false;
        }

        if (LiGetPendingAudioDuration() > 30) {
            return true;
        }

        const ma_uint32 frameCount = static_cast<ma_uint32>(
            static_cast<size_t>(bytesWritten) / frameBytes);
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            for (int i = 0;
                 i < MAX_BACKPRESSURE_WAIT_MS &&
                 ma_pcm_rb_available_write(&m_RingBuffer) < frameCount &&
                 !m_FatalError.load(std::memory_order_acquire);
                 i++) {
                m_QueueCondition.wait_for(lock, std::chrono::milliseconds(1));
            }
        }

        if (m_FatalError.load(std::memory_order_acquire)) {
            return false;
        }

        if (ma_pcm_rb_available_write(&m_RingBuffer) < frameCount) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Dropping audio frame because the WASAPI queue remained full");
            return true;
        }

        const uint8_t* source = reinterpret_cast<const uint8_t*>(m_DecodeBuffer.data());
        ma_uint32 framesRemaining = frameCount;
        while (framesRemaining > 0) {
            ma_uint32 framesToWrite = framesRemaining;
            void* destination = nullptr;
            if (ma_pcm_rb_acquire_write(&m_RingBuffer, &framesToWrite, &destination) != MA_SUCCESS ||
                    framesToWrite == 0) {
                m_FatalError.store(true, std::memory_order_release);
                return false;
            }

            std::memcpy(destination,
                        source,
                        static_cast<size_t>(framesToWrite) * frameBytes);
            if (ma_pcm_rb_commit_write(&m_RingBuffer, framesToWrite) != MA_SUCCESS) {
                m_FatalError.store(true, std::memory_order_release);
                return false;
            }

            source += static_cast<size_t>(framesToWrite) * frameBytes;
            framesRemaining -= framesToWrite;
        }
        return true;
    }

private:
    static void dataCallback(ma_device* device,
                             void* output,
                             const void*,
                             ma_uint32 frameCount)
    {
        auto* impl = static_cast<Impl*>(device->pUserData);
        const size_t frameBytes = static_cast<size_t>(impl->m_Config.channelCount) *
                                  sizeof(float);
        std::memset(output, 0, static_cast<size_t>(frameCount) * frameBytes);

        if (impl->m_FatalError.load(std::memory_order_acquire)) {
            return;
        }

        ma_uint32 framesRemaining = frameCount;
        uint8_t* destination = static_cast<uint8_t*>(output);
        bool readAny = false;
        while (framesRemaining > 0) {
            ma_uint32 framesToRead = framesRemaining;
            void* source = nullptr;
            if (ma_pcm_rb_acquire_read(&impl->m_RingBuffer, &framesToRead, &source) != MA_SUCCESS) {
                impl->m_FatalError.store(true, std::memory_order_release);
                return;
            }

            if (framesToRead == 0) {
                break;
            }

            std::memcpy(destination,
                        source,
                        static_cast<size_t>(framesToRead) * frameBytes);
            if (ma_pcm_rb_commit_read(&impl->m_RingBuffer, framesToRead) != MA_SUCCESS) {
                impl->m_FatalError.store(true, std::memory_order_release);
                return;
            }

            destination += static_cast<size_t>(framesToRead) * frameBytes;
            framesRemaining -= framesToRead;
            readAny = true;
        }
        if (readAny) {
            impl->m_QueueCondition.notify_all();
        }
    }

    static void notificationCallback(const ma_device_notification* notification)
    {
        if (notification->type == ma_device_notification_type_stopped ||
                notification->type == ma_device_notification_type_rerouted) {
            auto* impl = static_cast<Impl*>(notification->pDevice->pUserData);
            impl->m_FatalError.store(true, std::memory_order_release);
            impl->m_QueueCondition.notify_all();
        }
    }

    void stop()
    {
        if (m_DeviceInitialized) {
            ma_device_uninit(&m_Device);
            m_DeviceInitialized = false;
        }
        if (m_RingBufferInitialized) {
            ma_pcm_rb_uninit(&m_RingBuffer);
            m_RingBufferInitialized = false;
        }
        if (m_ContextInitialized) {
            ma_context_uninit(&m_Context);
            m_ContextInitialized = false;
        }
    }

    OPUS_MULTISTREAM_CONFIGURATION m_Config = {};
    ma_context m_Context = {};
    ma_device m_Device = {};
    ma_pcm_rb m_RingBuffer = {};
    ma_channel m_ChannelMap[MA_MAX_CHANNELS] = {};
    std::atomic<bool> m_FatalError {false};
    bool m_ContextInitialized = false;
    bool m_RingBufferInitialized = false;
    bool m_DeviceInitialized = false;
    std::mutex m_Mutex;
    std::condition_variable m_QueueCondition;
    std::vector<float> m_DecodeBuffer;
};

WasapiAudioRenderer::WasapiAudioRenderer()
    : m_Impl(std::make_unique<Impl>())
{
}

WasapiAudioRenderer::~WasapiAudioRenderer() = default;

bool WasapiAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    return m_Impl->prepare(opusConfig);
}

void* WasapiAudioRenderer::getAudioBuffer(int* size)
{
    return m_Impl->getBuffer(size);
}

bool WasapiAudioRenderer::submitAudio(int bytesWritten)
{
    return m_Impl->submit(bytesWritten);
}

IAudioRenderer::AudioFormat WasapiAudioRenderer::getAudioBufferFormat()
{
    return AudioFormat::Float32NE;
}
