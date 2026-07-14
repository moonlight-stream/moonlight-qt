#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "wasapi.h"

#include "SDL_compat.h"

#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <ks.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>

#include <Limelight.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace {

constexpr REFERENCE_TIME REFERENCE_TIMES_PER_SECOND = 10000000;
constexpr DWORD WASAPI_EVENT_TIMEOUT_MS = 2000;
constexpr int QUEUE_CAPACITY_MS = 100;
constexpr int MAX_BACKPRESSURE_WAIT_MS = 100;

const char* formatName(const WAVEFORMATEXTENSIBLE& format)
{
    if (format.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
        return "32-bit float";
    }

    if (format.Format.wBitsPerSample == 16) {
        return "16-bit PCM";
    }
    if (format.Format.wBitsPerSample == 24) {
        return "24-bit PCM";
    }
    if (format.Samples.wValidBitsPerSample == 24) {
        return "24-in-32-bit PCM";
    }
    return "32-bit PCM";
}

std::vector<DWORD> channelMasksForCount(int channelCount)
{
    switch (channelCount) {
    case 1:
        return {SPEAKER_FRONT_CENTER};
    case 2:
        return {KSAUDIO_SPEAKER_STEREO};
    case 6:
        // Limelight orders channels 4 and 5 as back left/right. Some Windows
        // drivers expose the equivalent 5.1 layout with side surrounds instead,
        // so probe both masks without changing interleaved channel order.
        return {KSAUDIO_SPEAKER_5POINT1, KSAUDIO_SPEAKER_5POINT1_SURROUND};
    case 8:
        return {KSAUDIO_SPEAKER_7POINT1_SURROUND};
    default:
        return {};
    }
}

WAVEFORMATEXTENSIBLE makeFormat(int sampleRate,
                                int channelCount,
                                DWORD channelMask,
                                WORD containerBits,
                                WORD validBits,
                                const GUID& subFormat)
{
    WAVEFORMATEXTENSIBLE format = {};
    format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    format.Format.nChannels = static_cast<WORD>(channelCount);
    format.Format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.Format.wBitsPerSample = containerBits;
    format.Format.nBlockAlign = static_cast<WORD>(channelCount * containerBits / 8);
    format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
    format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    format.Samples.wValidBitsPerSample = validBits;
    format.dwChannelMask = channelMask;
    format.SubFormat = subFormat;
    return format;
}

template<typename T>
void releaseCom(T*& object)
{
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

void logHresult(const char* operation, HRESULT result)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "%s failed: 0x%08lX",
                 operation,
                 static_cast<unsigned long>(result));
}

float clampSample(float sample)
{
    return std::max(-1.0f, std::min(1.0f, sample));
}

int32_t floatToSigned(float sample, int validBits)
{
    const float clamped = clampSample(sample);
    const int64_t positiveMax = (int64_t(1) << (validBits - 1)) - 1;
    const int64_t negativeMin = -(int64_t(1) << (validBits - 1));

    if (clamped <= -1.0f) {
        return static_cast<int32_t>(negativeMin);
    }

    return static_cast<int32_t>(std::llround(clamped * static_cast<double>(positiveMax)));
}

} // namespace

class WasapiAudioRenderer::Impl
{
public:
    Impl()
        : m_StopEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr))
    {
    }

    ~Impl()
    {
        stop();
        if (m_StopEvent != nullptr) {
            CloseHandle(m_StopEvent);
        }
    }

    bool prepare(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
    {
        if (opusConfig == nullptr || m_StopEvent == nullptr || m_RenderThread.joinable()) {
            return false;
        }

        if (opusConfig->sampleRate <= 0 || opusConfig->samplesPerFrame <= 0 ||
                channelMasksForCount(opusConfig->channelCount).empty()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unsupported WASAPI stream layout: %d Hz, %d channels",
                         opusConfig->sampleRate,
                         opusConfig->channelCount);
            return false;
        }

        m_Config = *opusConfig;
        m_DecodeBuffer.resize(static_cast<size_t>(m_Config.samplesPerFrame) *
                              m_Config.channelCount);

        m_RenderThread = std::thread(&Impl::renderThreadMain, this);

        std::unique_lock<std::mutex> lock(m_Mutex);
        m_InitCondition.wait(lock, [this]() { return m_InitializationComplete; });
        return m_InitializationSucceeded;
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

        if (bytesWritten < 0 ||
                static_cast<size_t>(bytesWritten) > m_DecodeBuffer.size() * sizeof(float)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Invalid WASAPI audio submission size: %d bytes",
                         bytesWritten);
            return false;
        }

        // Match the SDL renderer's latency control and avoid feeding local audio
        // while Moonlight already has excessive network-side audio queued.
        if (LiGetPendingAudioDuration() > 30) {
            return true;
        }

        std::unique_lock<std::mutex> lock(m_Mutex);
        for (int i = 0;
             i < MAX_BACKPRESSURE_WAIT_MS &&
             m_Queue.size() - m_QueuedBytes < static_cast<size_t>(bytesWritten) &&
             !m_FatalError.load(std::memory_order_acquire);
             i++) {
            m_QueueCondition.wait_for(lock, std::chrono::milliseconds(1));
        }

        if (m_FatalError.load(std::memory_order_acquire)) {
            return false;
        }

        if (m_Queue.size() - m_QueuedBytes < static_cast<size_t>(bytesWritten)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Dropping audio frame because the WASAPI queue remained full");
            return true;
        }

        writeQueue(reinterpret_cast<const uint8_t*>(m_DecodeBuffer.data()),
                   static_cast<size_t>(bytesWritten));
        return true;
    }

private:
    bool activateAudioClient(IMMDevice* device, IAudioClient** audioClient)
    {
        HRESULT result = device->Activate(__uuidof(IAudioClient),
                                          CLSCTX_ALL,
                                          nullptr,
                                          reinterpret_cast<void**>(audioClient));
        if (FAILED(result)) {
            logHresult("IMMDevice::Activate(IAudioClient)", result);
            return false;
        }
        return true;
    }

    bool chooseFormat(IAudioClient* audioClient, WAVEFORMATEXTENSIBLE& selectedFormat)
    {
        const auto channelMasks = channelMasksForCount(m_Config.channelCount);
        for (DWORD channelMask : channelMasks) {
            const std::vector<WAVEFORMATEXTENSIBLE> candidates = {
                makeFormat(m_Config.sampleRate, m_Config.channelCount, channelMask,
                           32, 32, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT),
                makeFormat(m_Config.sampleRate, m_Config.channelCount, channelMask,
                           32, 32, KSDATAFORMAT_SUBTYPE_PCM),
                makeFormat(m_Config.sampleRate, m_Config.channelCount, channelMask,
                           32, 24, KSDATAFORMAT_SUBTYPE_PCM),
                makeFormat(m_Config.sampleRate, m_Config.channelCount, channelMask,
                           24, 24, KSDATAFORMAT_SUBTYPE_PCM),
                makeFormat(m_Config.sampleRate, m_Config.channelCount, channelMask,
                           16, 16, KSDATAFORMAT_SUBTYPE_PCM),
            };

            for (const auto& candidate : candidates) {
                HRESULT result = audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                                &candidate.Format,
                                                                nullptr);
                if (result == S_OK) {
                    selectedFormat = candidate;
                    return true;
                }
            }
        }

        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "WASAPI device does not support %d Hz, %d-channel PCM in exclusive mode",
                     m_Config.sampleRate,
                     m_Config.channelCount);
        return false;
    }

    bool initializeStream(IMMDevice* device,
                          IAudioClient*& audioClient,
                          const WAVEFORMATEXTENSIBLE& format,
                          REFERENCE_TIME requestedPeriod)
    {
        HRESULT result = audioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                                 AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                 requestedPeriod,
                                                 requestedPeriod,
                                                 &format.Format,
                                                 nullptr);
        if (result != AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
            if (FAILED(result)) {
                logHresult("IAudioClient::Initialize(exclusive)", result);
                return false;
            }
            return true;
        }

        UINT32 alignedFrames = 0;
        result = audioClient->GetBufferSize(&alignedFrames);
        if (FAILED(result)) {
            logHresult("IAudioClient::GetBufferSize(alignment)", result);
            return false;
        }

        const REFERENCE_TIME alignedPeriod = static_cast<REFERENCE_TIME>(
            (static_cast<double>(alignedFrames) * REFERENCE_TIMES_PER_SECOND /
             format.Format.nSamplesPerSec) + 0.5);

        releaseCom(audioClient);
        if (!activateAudioClient(device, &audioClient)) {
            return false;
        }

        result = audioClient->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                         alignedPeriod,
                                         alignedPeriod,
                                         &format.Format,
                                         nullptr);
        if (FAILED(result)) {
            logHresult("IAudioClient::Initialize(aligned exclusive)", result);
            return false;
        }

        return true;
    }

    bool initializeWasapi(IMMDeviceEnumerator*& enumerator,
                          IMMDevice*& device,
                          IAudioClient*& audioClient,
                          IAudioRenderClient*& renderClient,
                          HANDLE& audioEvent,
                          WAVEFORMATEXTENSIBLE& selectedFormat,
                          UINT32& bufferFrameCount)
    {
        HRESULT result = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                          nullptr,
                                          CLSCTX_ALL,
                                          __uuidof(IMMDeviceEnumerator),
                                          reinterpret_cast<void**>(&enumerator));
        if (FAILED(result)) {
            logHresult("CoCreateInstance(MMDeviceEnumerator)", result);
            return false;
        }

        result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(result)) {
            logHresult("IMMDeviceEnumerator::GetDefaultAudioEndpoint", result);
            return false;
        }

        if (!activateAudioClient(device, &audioClient) ||
                !chooseFormat(audioClient, selectedFormat)) {
            return false;
        }

        REFERENCE_TIME defaultPeriod = 0;
        result = audioClient->GetDevicePeriod(&defaultPeriod, nullptr);
        if (FAILED(result)) {
            logHresult("IAudioClient::GetDevicePeriod", result);
            return false;
        }

        // The default period is intentionally preferred over the minimum period.
        // Moonlight values stable playback over workstation-class minimum latency.
        if (!initializeStream(device, audioClient, selectedFormat, defaultPeriod)) {
            return false;
        }

        audioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (audioEvent == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CreateEvent for WASAPI buffer notifications failed: %lu",
                         GetLastError());
            return false;
        }

        result = audioClient->SetEventHandle(audioEvent);
        if (FAILED(result)) {
            logHresult("IAudioClient::SetEventHandle", result);
            return false;
        }

        result = audioClient->GetBufferSize(&bufferFrameCount);
        if (FAILED(result)) {
            logHresult("IAudioClient::GetBufferSize", result);
            return false;
        }

        result = audioClient->GetService(__uuidof(IAudioRenderClient),
                                         reinterpret_cast<void**>(&renderClient));
        if (FAILED(result)) {
            logHresult("IAudioClient::GetService(IAudioRenderClient)", result);
            return false;
        }

        const size_t sourceBytesPerSecond = static_cast<size_t>(m_Config.sampleRate) *
                                            m_Config.channelCount * sizeof(float);
        const size_t endpointSourceBytes = static_cast<size_t>(bufferFrameCount) *
                                           m_Config.channelCount * sizeof(float);
        const size_t queueCapacity = std::max(sourceBytesPerSecond * QUEUE_CAPACITY_MS / 1000,
                                              endpointSourceBytes * 4);
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Queue.assign(queueCapacity, 0);
            m_ReadPosition = 0;
            m_WritePosition = 0;
            m_QueuedBytes = 0;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "WASAPI exclusive audio: %d Hz, %d channels, %s, %u-frame period",
                    m_Config.sampleRate,
                    m_Config.channelCount,
                    formatName(selectedFormat),
                    bufferFrameCount);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "WASAPI channel mask: 0x%08lX",
                    static_cast<unsigned long>(selectedFormat.dwChannelMask));
        return true;
    }

    void renderThreadMain()
    {
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* audioClient = nullptr;
        IAudioRenderClient* renderClient = nullptr;
        HANDLE audioEvent = nullptr;
        HANDLE mmcssTask = nullptr;
        DWORD mmcssTaskIndex = 0;
        UINT32 bufferFrameCount = 0;
        WAVEFORMATEXTENSIBLE selectedFormat = {};
        bool streamStarted = false;

        HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool comInitialized = SUCCEEDED(comResult);
        if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) {
            logHresult("CoInitializeEx", comResult);
            completeInitialization(false);
            return;
        }

        if (!initializeWasapi(enumerator,
                              device,
                              audioClient,
                              renderClient,
                              audioEvent,
                              selectedFormat,
                              bufferFrameCount)) {
            completeInitialization(false);
            cleanupWasapi(audioClient, renderClient, device, enumerator, audioEvent,
                          mmcssTask, streamStarted, comInitialized);
            return;
        }

        std::vector<float> sourceBuffer(static_cast<size_t>(bufferFrameCount) *
                                        m_Config.channelCount);

        BYTE* endpointBuffer = nullptr;
        HRESULT result = renderClient->GetBuffer(bufferFrameCount, &endpointBuffer);
        if (FAILED(result)) {
            logHresult("IAudioRenderClient::GetBuffer(initial)", result);
            completeInitialization(false);
            cleanupWasapi(audioClient, renderClient, device, enumerator, audioEvent,
                          mmcssTask, streamStarted, comInitialized);
            return;
        }
        result = renderClient->ReleaseBuffer(bufferFrameCount, AUDCLNT_BUFFERFLAGS_SILENT);
        if (FAILED(result)) {
            logHresult("IAudioRenderClient::ReleaseBuffer(initial)", result);
            completeInitialization(false);
            cleanupWasapi(audioClient, renderClient, device, enumerator, audioEvent,
                          mmcssTask, streamStarted, comInitialized);
            return;
        }

        mmcssTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &mmcssTaskIndex);
        if (mmcssTask == nullptr) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Unable to register WASAPI thread with MMCSS: %lu",
                        GetLastError());
        }

        result = audioClient->Start();
        if (FAILED(result)) {
            logHresult("IAudioClient::Start", result);
            completeInitialization(false);
            cleanupWasapi(audioClient, renderClient, device, enumerator, audioEvent,
                          mmcssTask, streamStarted, comInitialized);
            return;
        }
        streamStarted = true;
        completeInitialization(true);

        HANDLE waitHandles[] = {m_StopEvent, audioEvent};
        while (true) {
            DWORD waitResult = WaitForMultipleObjects(2,
                                                      waitHandles,
                                                      FALSE,
                                                      WASAPI_EVENT_TIMEOUT_MS);
            if (waitResult == WAIT_OBJECT_0) {
                break;
            }
            if (waitResult != WAIT_OBJECT_0 + 1) {
                if (waitResult == WAIT_TIMEOUT) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Timed out waiting for a WASAPI render event");
                }
                else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "WaitForMultipleObjects for WASAPI failed: %lu",
                                 GetLastError());
                }
                m_FatalError.store(true, std::memory_order_release);
                break;
            }

            const size_t sourceBytesNeeded = sourceBuffer.size() * sizeof(float);
            size_t sourceBytesRead = 0;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                sourceBytesRead = readQueue(reinterpret_cast<uint8_t*>(sourceBuffer.data()),
                                            sourceBytesNeeded);
            }
            m_QueueCondition.notify_all();

            if (sourceBytesRead < sourceBytesNeeded) {
                std::memset(reinterpret_cast<uint8_t*>(sourceBuffer.data()) + sourceBytesRead,
                            0,
                            sourceBytesNeeded - sourceBytesRead);
            }

            result = renderClient->GetBuffer(bufferFrameCount, &endpointBuffer);
            if (FAILED(result)) {
                logHresult("IAudioRenderClient::GetBuffer", result);
                m_FatalError.store(true, std::memory_order_release);
                break;
            }

            if (sourceBytesRead == 0) {
                result = renderClient->ReleaseBuffer(bufferFrameCount,
                                                     AUDCLNT_BUFFERFLAGS_SILENT);
            }
            else {
                convertSamples(sourceBuffer, endpointBuffer, selectedFormat);
                result = renderClient->ReleaseBuffer(bufferFrameCount, 0);
            }

            if (FAILED(result)) {
                logHresult("IAudioRenderClient::ReleaseBuffer", result);
                m_FatalError.store(true, std::memory_order_release);
                break;
            }
        }

        m_QueueCondition.notify_all();
        cleanupWasapi(audioClient, renderClient, device, enumerator, audioEvent,
                      mmcssTask, streamStarted, comInitialized);
    }

    void cleanupWasapi(IAudioClient*& audioClient,
                       IAudioRenderClient*& renderClient,
                       IMMDevice*& device,
                       IMMDeviceEnumerator*& enumerator,
                       HANDLE& audioEvent,
                       HANDLE& mmcssTask,
                       bool streamStarted,
                       bool comInitialized)
    {
        if (streamStarted && audioClient != nullptr) {
            audioClient->Stop();
        }
        if (mmcssTask != nullptr) {
            AvRevertMmThreadCharacteristics(mmcssTask);
            mmcssTask = nullptr;
        }
        releaseCom(renderClient);
        releaseCom(audioClient);
        releaseCom(device);
        releaseCom(enumerator);
        if (audioEvent != nullptr) {
            CloseHandle(audioEvent);
            audioEvent = nullptr;
        }
        if (comInitialized) {
            CoUninitialize();
        }
    }

    void convertSamples(const std::vector<float>& source,
                        BYTE* destination,
                        const WAVEFORMATEXTENSIBLE& format)
    {
        if (format.SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            std::memcpy(destination, source.data(), source.size() * sizeof(float));
            return;
        }

        if (format.Format.wBitsPerSample == 16) {
            int16_t* output = reinterpret_cast<int16_t*>(destination);
            for (size_t i = 0; i < source.size(); i++) {
                output[i] = static_cast<int16_t>(floatToSigned(source[i], 16));
            }
            return;
        }

        if (format.Format.wBitsPerSample == 24) {
            for (size_t i = 0; i < source.size(); i++) {
                const int32_t value = floatToSigned(source[i], 24);
                destination[i * 3] = static_cast<BYTE>(value & 0xFF);
                destination[i * 3 + 1] = static_cast<BYTE>((value >> 8) & 0xFF);
                destination[i * 3 + 2] = static_cast<BYTE>((value >> 16) & 0xFF);
            }
            return;
        }

        int32_t* output = reinterpret_cast<int32_t*>(destination);
        if (format.Samples.wValidBitsPerSample == 24) {
            for (size_t i = 0; i < source.size(); i++) {
                output[i] = static_cast<int32_t>(
                    static_cast<int64_t>(floatToSigned(source[i], 24)) * 256);
            }
        }
        else {
            for (size_t i = 0; i < source.size(); i++) {
                output[i] = floatToSigned(source[i], 32);
            }
        }
    }

    void completeInitialization(bool success)
    {
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_InitializationSucceeded = success;
            m_InitializationComplete = true;
            if (!success) {
                m_FatalError.store(true, std::memory_order_release);
            }
        }
        m_InitCondition.notify_all();
    }

    void stop()
    {
        if (m_StopEvent != nullptr) {
            SetEvent(m_StopEvent);
        }
        m_QueueCondition.notify_all();
        if (m_RenderThread.joinable()) {
            m_RenderThread.join();
        }
    }

    void writeQueue(const uint8_t* source, size_t byteCount)
    {
        const size_t firstPart = std::min(byteCount, m_Queue.size() - m_WritePosition);
        std::memcpy(m_Queue.data() + m_WritePosition, source, firstPart);
        std::memcpy(m_Queue.data(), source + firstPart, byteCount - firstPart);
        m_WritePosition = (m_WritePosition + byteCount) % m_Queue.size();
        m_QueuedBytes += byteCount;
    }

    size_t readQueue(uint8_t* destination, size_t byteCount)
    {
        const size_t bytesToRead = std::min(byteCount, m_QueuedBytes);
        if (bytesToRead == 0) {
            return 0;
        }

        const size_t firstPart = std::min(bytesToRead, m_Queue.size() - m_ReadPosition);
        std::memcpy(destination, m_Queue.data() + m_ReadPosition, firstPart);
        std::memcpy(destination + firstPart, m_Queue.data(), bytesToRead - firstPart);
        m_ReadPosition = (m_ReadPosition + bytesToRead) % m_Queue.size();
        m_QueuedBytes -= bytesToRead;
        return bytesToRead;
    }

    OPUS_MULTISTREAM_CONFIGURATION m_Config = {};
    HANDLE m_StopEvent = nullptr;
    std::thread m_RenderThread;
    std::atomic<bool> m_FatalError {false};

    std::mutex m_Mutex;
    std::condition_variable m_InitCondition;
    std::condition_variable m_QueueCondition;
    bool m_InitializationComplete = false;
    bool m_InitializationSucceeded = false;

    std::vector<float> m_DecodeBuffer;
    std::vector<uint8_t> m_Queue;
    size_t m_ReadPosition = 0;
    size_t m_WritePosition = 0;
    size_t m_QueuedBytes = 0;
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
