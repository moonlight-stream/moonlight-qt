#include "coreaudio.h"
#include "coreaudio_helpers.h"
#include "settings/streamingpreferences.h"

#if TARGET_OS_OSX
#include <IOKit/audio/IOAudioTypes.h>
#endif

#include <Accelerate/Accelerate.h>
#include <QtGlobal>
#include <SDL.h>
#include <string>

#define kRingBufferMaxSeconds 0.030

CoreAudioRenderer::CoreAudioRenderer()
    : m_SpatialBuffer(2, 4096)
{
    DEBUG_TRACE("CoreAudioRenderer construct");

    AudioComponentDescription description;
    description.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
    description.componentSubType = kAudioUnitSubType_RemoteIO;
#elif TARGET_OS_OSX
    description.componentSubType = kAudioUnitSubType_HALOutput;
#endif
    description.componentManufacturer = kAudioUnitManufacturer_Apple;
    description.componentFlags = 0;
    description.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(NULL, &description);
    if (!comp) {
        return;
    }

    OSStatus status = AudioComponentInstanceNew(comp, &m_OutputAU);
    if (status != noErr) {
        CA_LogError(status, "Failed to create an instance of HALOutput or RemoteIO");
        throw std::runtime_error("Failed to create an instance of HALOutput or RemoteIO");
    }
}

CoreAudioRenderer::~CoreAudioRenderer()
{
    DEBUG_TRACE("CoreAudioRenderer destruct");
    cleanup();
}

void CoreAudioRenderer::stop()
{
    DEBUG_TRACE("CoreAudioRenderer stop");
    if (m_OutputAU != nullptr) {
        AudioOutputUnitStop(m_OutputAU);
    }
}

void CoreAudioRenderer::cleanup()
{
    DEBUG_TRACE("CoreAudioRenderer cleanup");
    stop();

    if (m_OutputAU != nullptr) {
        AudioUnitUninitialize(m_OutputAU);
        AudioComponentInstanceDispose(m_OutputAU);
        m_OutputAU = nullptr;

        // Must be destroyed after the stream is stopped
        TPCircularBufferCleanup(&m_RingBuffer);
    }

    if (m_OutputDeviceID) {
        deinitListeners();
        m_OutputDeviceID = 0;
    }

    if (m_OutputDeviceName) {
        free(m_OutputDeviceName);
    }
}

int CoreAudioRenderer::getCapabilities()
{
    // CAPABILITY_DIRECT_SUBMIT feels worse than decoding in a separate thread
    return CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION;
}

IAudioRenderer::AudioFormat CoreAudioRenderer::getAudioBufferFormat()
{
    return AudioFormat::Float32NE;
}

void CoreAudioRenderer::statsIncDeviceOverload()
{
    m_ActiveWndAudioStats.totalGlitches++;
}

// realtime method
void CoreAudioRenderer::statsTrackRender(uint64_t startTimeUs, const AudioTimeStamp *inTimestamp, uint32_t inNumberFrames)
{
    // check for lost packets because we weren't called in time, possibly due to system overload
    if (m_LastSampleTime && inTimestamp->mFlags & kAudioTimeStampSampleTimeValid) {
        double expectedSampleTime = m_LastSampleTime + inNumberFrames;
        if (expectedSampleTime != inTimestamp->mSampleTime) {
            uint32_t lostFrames = (inTimestamp->mSampleTime - m_LastSampleTime) - m_LastNumFrames;
            double lostDuration = (double)lostFrames / 48000.0;

            m_ActiveWndAudioStats.totalGlitches++;

#ifdef COREAUDIO_DEBUG
            dispatch_async(dispatch_get_main_queue(), ^{
                DEBUG_TRACE("[%llu] Error: lost/dropped audio frames: %u (%.02fms)", inTimestamp->mHostTime, lostFrames, lostDuration * 1000.0);
            });
#endif
        }
    }

    m_LastSampleTime = inTimestamp->mSampleTime;
    m_LastNumFrames = inNumberFrames;

    // add this to our decoderTime
    uint64_t decodeTimeUs = LiGetMicroseconds() - startTimeUs;
    m_ActiveWndAudioStats.decodeDurationUs += decodeTimeUs;

    // We now have decodeDurationUs covering 2 time periods:
    // 1. Filling the queue: Opus decoding plus write to circular buffer (statsTrackDecodeTime)
    // 2. Emptying the queue: from start of AudioUnit callback in either direct or spatial mode (statsTrackRender)
    //    Although it's referred to as render time, the time is just added to decodeDurationUs
}

int CoreAudioRenderer::stringifyAudioStats(AUDIO_STATS& stats, char *output, int length)
{
    // let parent class provide generic stats first
    int offset = IAudioRenderer::stringifyAudioStats(stats, output, length);
    if (offset < 0) {
        return -1;
    }

    int ret = snprintf(
        &output[offset],
        length - offset,
        "Output device: %s @ %.1f kHz, %u-channel\n"
        "Render mode: %s %s %s %s\n"
        "Latency: %0.1f ms (network %d ms, buffers %.1f ms, hardware: %.1f ms)\n",

        // "Output device: %s @ %.1f kHz, %u-channel\n"
        m_OutputDeviceName,
        m_OutputASBD.mSampleRate / 1000.0,
        m_OutputASBD.mChannelsPerFrame,

        // "Render mode: %s %s %s %s\n"
        m_Spatial ? (m_SpatialAU.m_PersonalizedHRTF ? "personalized spatial audio" : "spatial audio") : "passthrough",
        m_Spatial && m_SpatialAU.m_HeadTracking ? "with head-tracking for" : "for",
        !strcmp(m_OutputTransportType, "blue") ? "Bluetooth"
            : !strcmp(m_OutputTransportType, "bltn") ? "built-in"
            : !strcmp(m_OutputTransportType, "usb ") ? "USB"
            : !strcmp(m_OutputTransportType, "hdmi") ? "HDMI"
            : !strcmp(m_OutputTransportType, "airp") ? "AirPlay"
            : m_OutputTransportType,
        !strcmp(m_OutputDataSource      , "hdpn") ? "headphones"
            : !strcmp(m_OutputDataSource, "ispk") ? "internal speakers"
            : !strcmp(m_OutputDataSource, "espk") ? "external speakers"
            : m_OutputDataSource,

        // "Latency: %0.1fms (network %dms, buffers %.1fms, hardware: %.1fms)\n"
        (double)stats.lastRtt + (m_TotalSoftwareLatency + m_OutputHardwareLatency) * 1000.0,
        stats.lastRtt,
        m_TotalSoftwareLatency * 1000.0,
        m_OutputHardwareLatency * 1000.0
    );
    if (ret < 0 || ret >= length - offset) {
        SDL_assert(false);
        return -1;
    }

    return offset + ret;
}

// realtime method
OSStatus renderCallbackDirect(void *inRefCon,
                                     AudioUnitRenderActionFlags *ioActionFlags,
                                     const AudioTimeStamp *inTimestamp,
                                     uint32_t /*inBusNumber*/,
                                     uint32_t inNumberFrames,
                                     AudioBufferList *ioData)
{
    uint64_t start = LiGetMicroseconds();

    CoreAudioRenderer *me = (CoreAudioRenderer *)inRefCon;
    int bytesToCopy = ioData->mBuffers[0].mDataByteSize;
    float *targetBuffer = (float *)ioData->mBuffers[0].mData;

    // Pull audio from playthrough buffer
    uint32_t availableBytes;
    float *buffer = (float *)TPCircularBufferTail(&me->m_RingBuffer, &availableBytes);

    if ((int)availableBytes < bytesToCopy) {
        // write silence if not enough buffered data is available
        // faster version of memset(targetBuffer, 0, bytesToCopy);
        vDSP_vclr(targetBuffer, 1, bytesToCopy);
        *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
    } else {
        // faster version of memcpy(targetBuffer, buffer, qMin(bytesToCopy, (int)availableBytes));
        vDSP_mmov(buffer, targetBuffer, 1, qMin(bytesToCopy, (int)availableBytes), 1, 1);
        TPCircularBufferConsume(&me->m_RingBuffer, qMin(bytesToCopy, (int)availableBytes));
    }

    me->statsTrackRender(start, inTimestamp, inNumberFrames);

    return noErr;
}

// realtime method
OSStatus renderCallbackSpatial(void *inRefCon,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *inTimestamp,
                                      uint32_t /*inBusNumber*/,
                                      uint32_t inNumberFrames,
                                      AudioBufferList *ioData)
{
    uint64_t start = LiGetMicroseconds();
    CoreAudioRenderer *me = (CoreAudioRenderer *)inRefCon;
    AudioBufferList *spatialBuffer = me->m_SpatialBuffer.get();

    // Set the byte size with the output audio buffer list.
    for (uint32_t i = 0; i < spatialBuffer->mNumberBuffers; i++) {
        spatialBuffer->mBuffers[i].mDataByteSize = inNumberFrames * 4;
    }

    // Process the input frames with the audio unit spatial mixer.
    me->m_SpatialAU.process(spatialBuffer, ioActionFlags, inTimestamp, inNumberFrames);

    // Copy the temporary buffer to the output.
    for (uint32_t i = 0; i < spatialBuffer->mNumberBuffers; i++) {
        // faster version of memcpy(ioData->mBuffers[i].mData, spatialBuffer->mBuffers[i].mData, inNumberFrames * 4);
        vDSP_mmov((const float *)spatialBuffer->mBuffers[i].mData, (float *)ioData->mBuffers[i].mData, 1, inNumberFrames * 4, 1, 1);
    }

    me->statsTrackRender(start, inTimestamp, inNumberFrames);

    return noErr;
}

bool CoreAudioRenderer::prepareForPlayback(const OPUS_MULTISTREAM_CONFIGURATION* opusConfig)
{
    OSStatus status = noErr;
    m_opusConfig = opusConfig;

    // Request the OS set our buffer close to the Opus packet size
    m_AudioPacketDuration = (opusConfig->samplesPerFrame / (opusConfig->sampleRate / 1000)) / 1000.0;

    if (!initAudioUnit()) {
        DEBUG_TRACE("initAudioUnit failed");
        return false;
    }

    if (!initRingBuffer()) {
        DEBUG_TRACE("initRingBuffer failed");
        return false;
    }

    if (!initListeners()) {
        DEBUG_TRACE("initListeners failed");
        return false;
    }

    m_Spatial = false;
    AUSpatialMixerOutputType outputType = getSpatialMixerOutputType();

    DEBUG_TRACE("CoreAudioRenderer getSpatialMixerOutputType = %d", outputType);

    if (opusConfig->channelCount > 2) {
        if (outputType != kSpatialMixerOutputType_ExternalSpeakers) {
            m_Spatial = true;
        }
    }

    StreamingPreferences *prefs = StreamingPreferences::get();
    if (prefs->spatialAudioConfig == StreamingPreferences::SAC_DISABLED) {
        // User has disabled spatial audio
        DEBUG_TRACE("CoreAudioRenderer user has disabled spatial audio");
        m_Spatial = false;
    }

    // indicate the format our callback will provide samples in
    // If necessary, the OS takes care of resampling (but not downmixing, hmm)
    AudioStreamBasicDescription streamDesc;
    memset(&streamDesc, 0, sizeof(AudioStreamBasicDescription));
    streamDesc.mSampleRate       = m_opusConfig->sampleRate;
    streamDesc.mFormatID         = kAudioFormatLinearPCM;
    streamDesc.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
    streamDesc.mFramesPerPacket  = 1;
    streamDesc.mChannelsPerFrame = (uint32_t)opusConfig->channelCount;
    streamDesc.mBitsPerChannel   = 32;
    streamDesc.mBytesPerPacket   = 4 * opusConfig->channelCount;
    streamDesc.mBytesPerFrame    = streamDesc.mBytesPerPacket;

    if (m_Spatial) {
        // render audio for binaural headphones or built-in laptop speakers
        setCallback(renderCallbackSpatial);

        // this callback is non-interleaved
        streamDesc.mFormatFlags    |= kAudioFormatFlagIsNonInterleaved;
        streamDesc.mBytesPerPacket = 4;
        streamDesc.mBytesPerFrame  = 4;

        m_SpatialOutputType = outputType;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "CoreAudioRenderer is using spatial audio output");

        if (!m_SpatialAU.setup(outputType, opusConfig->sampleRate, opusConfig->channelCount)) {
            DEBUG_TRACE("m_SpatialAU.setup failed");
            return false;
        }

        m_TotalSoftwareLatency += m_SpatialAU.getAudioUnitLatency();
    } else {
        // direct passthrough of all channels for stereo and HDMI
        setCallback(renderCallbackDirect);

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "CoreAudioRenderer is using passthrough mode");
    }

    status = AudioUnitSetProperty(m_OutputAU, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamDesc, sizeof(streamDesc));
    if (status != noErr) {
        CA_LogError(status, "Failed to set output stream format");
        return false;
    }

    DEBUG_TRACE("CoreAudioRenderer start");
    status = AudioOutputUnitStart(m_OutputAU);
    if (status != noErr) {
        CA_LogError(status, "Failed to start output audio unit");
        return false;
    }

    return true;
}

bool CoreAudioRenderer::initAudioUnit()
{
    // Initialize the audio unit interface to begin configuring it.
    OSStatus status = AudioUnitInitialize(m_OutputAU);
    if (status != noErr) {
        CA_LogError(status, "Failed to initialize the output audio unit");
        return false;
    }

    /* macOS:
     * disable OutputAU input IO
     * enable OutputAU output IO
     * get system default output AudioDeviceID  (todo: allow user to choose specific device from list)
     * set OutputAU to AudioDeviceID
     * get device's AudioStreamBasicDescription (format, bit depth, samplerate, etc)
     * get device name
     * get output buffer frame size
     * get output buffer min/max
     * set output buffer frame size
     */

#if TARGET_OS_OSX
    constexpr AudioUnitElement outputElement{0};
    constexpr AudioUnitElement inputElement{1};

    {
        uint32_t enableIO = 0;
        status = AudioUnitSetProperty(m_OutputAU, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputElement, &enableIO, sizeof(enableIO));
        if (status != noErr) {
            CA_LogError(status, "Failed to disable the input on AUHAL");
            return false;
        }

        enableIO = 1;
        status = AudioUnitSetProperty(m_OutputAU, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, outputElement, &enableIO, sizeof(enableIO));
        if (status != noErr) {
            CA_LogError(status, "Failed to enable the output on AUHAL");
            return false;
        }
    }

    {
        uint32_t size = sizeof(AudioDeviceID);
        AudioObjectPropertyAddress addr{kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &addr, outputElement, nil, &size, &m_OutputDeviceID);
        if (status != noErr) {
            CA_LogError(status, "Failed to get the default output device");
            return false;
        }
    }

    {
        CFStringRef name;
        uint32_t nameSize = sizeof(CFStringRef);
        AudioObjectPropertyAddress addr{kAudioObjectPropertyName, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addr, 0, nil, &nameSize, &name);
        if (status != noErr) {
            CA_LogError(status, "Failed to get name of output device");
            return false;
        }
        setOutputDeviceName(name);
        CFRelease(name);
        DEBUG_TRACE("CoreAudioRenderer default output device ID: %d, name: %s", m_OutputDeviceID, m_OutputDeviceName);
    }

    {
        // Set the current device to the default output device.
        // This should be done only after I/O is enabled on the output audio unit.
        status = AudioUnitSetProperty(m_OutputAU, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, outputElement, &m_OutputDeviceID, sizeof(AudioDeviceID));
        if (status != noErr) {
            CA_LogError(status, "Failed to set the default output device");
            return false;
        }
    }

    {
        uint32_t streamFormatSize = sizeof(AudioStreamBasicDescription);
        AudioObjectPropertyAddress addr{kAudioDevicePropertyStreamFormat, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addr, 0, nil, &streamFormatSize, &m_OutputASBD);
        if (status != noErr) {
            CA_LogError(status, "Failed to get output device AudioStreamBasicDescription");
            return false;
        }
        CA_PrintASBD("CoreAudioRenderer output format:", &m_OutputASBD);
    }

    // Buffer:
    // The goal here is to set the system buffer to our desired value, which is currently in m_AudioPacketDuration.
    // First we get the current value, and the range of allowed values, set our value, and then query to find the actual value.
    // We also query the hardware latency (e.g. Bluetooth delay for AirPods), but this is just for fun

    {
        uint32_t bufferFrameSize = 0;
        uint32_t size = sizeof(uint32_t);
        AudioObjectPropertyAddress addr{kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addr, 0, nil, &size, &bufferFrameSize);
        if (status != noErr) {
            CA_LogError(status, "Failed to get the output device buffer frame size");
            return false;
        }
        DEBUG_TRACE("CoreAudioRenderer output current BufferFrameSize %d", bufferFrameSize);
    }

    {
        AudioValueRange avr;
        uint32_t size = sizeof(AudioValueRange);
        AudioObjectPropertyAddress addr{kAudioDevicePropertyBufferFrameSizeRange, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addr, 0, nil, &size, &avr);
        if (status != noErr) {
            CA_LogError(status, "Failed to get the output device buffer frame size range");
            return false;
        }
        m_OutputSoftwareLatencyMin = avr.mMinimum / m_OutputASBD.mSampleRate;
        m_OutputSoftwareLatencyMax = avr.mMaximum / m_OutputASBD.mSampleRate;
        DEBUG_TRACE("CoreAudioRenderer output BufferFrameSizeRange: %.0f - %.0f", avr.mMinimum, avr.mMaximum);
    }

    // The latency values we have access to are:
    // kAudioDevicePropertyBufferFrameSize    our requested buffer as close to Opus packet size as possible
    //   + kAudioDevicePropertySafetyOffset   an additional CoreAudio buffer
    //   + kAudioUnitProperty_Latency         processing latency of OutputAU (+ SpatialAU in spatial mode)
    //   = total software latency
    // kAudioDevicePropertyLatency = hardware latency

    {
        double desiredBufferFrameSize = m_AudioPacketDuration;
        desiredBufferFrameSize = qMax(qMin(desiredBufferFrameSize, m_OutputSoftwareLatencyMax), m_OutputSoftwareLatencyMin);
        uint32_t bufferFrameSize = (uint32_t)(desiredBufferFrameSize * m_OutputASBD.mSampleRate);
        AudioObjectPropertyAddress addrSet{kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeInput, kAudioObjectPropertyElementMain};
        status = AudioObjectSetPropertyData(m_OutputDeviceID, &addrSet, 0, NULL, sizeof(uint32_t), &bufferFrameSize);
        if (status != noErr) {
            CA_LogError(status, "Failed to set the output device buffer frame size");
            return false;
        }
        DEBUG_TRACE("CoreAudioRenderer output requested BufferFrameSize of %d (%0.3f ms)", bufferFrameSize, desiredBufferFrameSize * 1000.0);

        // see what we got
        uint32_t size = sizeof(uint32_t);
        AudioObjectPropertyAddress addrGet{kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addrGet, 0, nil, &size, &m_BufferFrameSize);
        if (status != noErr) {
            CA_LogError(status, "Failed to get the output device buffer frame size");
            return false;
        }
        double bufferFrameLatency = (double)m_BufferFrameSize / m_OutputASBD.mSampleRate;
        m_TotalSoftwareLatency += bufferFrameLatency;
        m_TotalSoftwareLatency += 0.0025; // Opus has 2.5ms of initial delay
        DEBUG_TRACE("CoreAudioRenderer output now has actual BufferFrameSize of %d (%0.3f ms)", m_BufferFrameSize, bufferFrameLatency * 1000.0);
    }

    {
        double audioUnitLatency = 0.0;
        uint32_t size = sizeof(audioUnitLatency);
        status = AudioUnitGetProperty(m_OutputAU, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &audioUnitLatency, &size);
        if (status != noErr) {
            CA_LogError(status, "Failed to get OutputAU AudioUnit latency");
            return false;
        }
        m_TotalSoftwareLatency += audioUnitLatency;
        DEBUG_TRACE("CoreAudioRenderer OutputAU AudioUnit latency: %0.2f ms", audioUnitLatency * 1000.0);
    }

    {
        uint32_t safetyOffsetLatency = 0;
        uint32_t size = sizeof(safetyOffsetLatency);
        AudioObjectPropertyAddress addrGet{kAudioDevicePropertySafetyOffset, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addrGet, 0, nil, &size, &safetyOffsetLatency);
        if (status != noErr) {
            CA_LogError(status, "Failed to get safety offset latency");
            return false;
        }
        m_TotalSoftwareLatency += (double)safetyOffsetLatency / m_OutputASBD.mSampleRate;
        DEBUG_TRACE("CoreAudioRenderer OutputAU safety latency: %0.2f ms", ((double)safetyOffsetLatency / m_OutputASBD.mSampleRate) * 1000.0);
    }

    {
        uint32_t latencyFrames;
        uint32_t size = sizeof(uint32_t);
        AudioObjectPropertyAddress addr{kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
        status = AudioObjectGetPropertyData(m_OutputDeviceID, &addr, 0, nil, &size, &latencyFrames);
        if (status != noErr) {
            CA_LogError(status, "Failed to get the output device hardware latency");
            return false;
        }
        m_OutputHardwareLatency = (double)latencyFrames / m_OutputASBD.mSampleRate;
        DEBUG_TRACE("CoreAudioRenderer output hardware latency: %d (%0.2f ms)", latencyFrames, m_OutputHardwareLatency * 1000.0);
    }
#endif

    return true;
}

bool CoreAudioRenderer::initRingBuffer()
{
    // Always buffer at least 2 packets, up to 30ms worth of packets
    int packetsToBuffer = qMax(2, (int)ceil(kRingBufferMaxSeconds / m_AudioPacketDuration));

    bool ok = TPCircularBufferInit(&m_RingBuffer,
                                   sizeof(float) *
                                   m_opusConfig->channelCount *
                                   m_opusConfig->samplesPerFrame *
                                   packetsToBuffer);
    if (!ok) return false;

    // Spatial mixer code needs to be able to read from the ring buffer
    m_SpatialAU.setRingBufferPtr(&m_RingBuffer);

    // real length will be larger than requested due to memory page alignment
    m_BufferSize = m_RingBuffer.length;
    DEBUG_TRACE("CoreAudioRenderer ring buffer init, %d packets (%d bytes)", packetsToBuffer, m_BufferSize);

    return true;
}

OSStatus onDeviceOverload(AudioObjectID /*inObjectID*/,
                          UInt32 /*inNumberAddresses*/,
                          const AudioObjectPropertyAddress * /*inAddresses*/,
                          void *inClientData)
{
    CoreAudioRenderer *me = (CoreAudioRenderer *)inClientData;
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CoreAudioRenderer output device overload");
    me->statsIncDeviceOverload();
    return noErr;
}

OSStatus onAudioNeedsReinit(AudioObjectID /*inObjectID*/,
                            UInt32 /*inNumberAddresses*/,
                            const AudioObjectPropertyAddress * /*inAddresses*/,
                            void *inClientData)
{
    CoreAudioRenderer *me = (CoreAudioRenderer *)inClientData;
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "CoreAudioRenderer output device had a change, will reinit");
    me->m_needsReinit = true;
    return noErr;
}

bool CoreAudioRenderer::initListeners()
{
    // events we care about on our output device

    AudioObjectPropertyAddress addr{kAudioDeviceProcessorOverload, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    OSStatus status = AudioObjectAddPropertyListener(m_OutputDeviceID, &addr, onDeviceOverload, this);
    if (status != noErr) {
        CA_LogError(status, "Failed to add listener for kAudioDeviceProcessorOverload");
        return false;
    }

    addr.mSelector = kAudioDevicePropertyDeviceHasChanged;
    status = AudioObjectAddPropertyListener(m_OutputDeviceID, &addr, onAudioNeedsReinit, this);
    if (status != noErr) {
        CA_LogError(status, "Failed to add listener for kAudioDevicePropertyDeviceHasChanged");
        return false;
    }

    // non-device-specific listeners
    addr.mSelector = kAudioHardwarePropertyServiceRestarted;
    status = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr, onAudioNeedsReinit, this);
    if (status != noErr) {
        CA_LogError(status, "Failed to add listener for kAudioHardwarePropertyServiceRestarted");
        return false;
    }

    addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    status = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &addr, onAudioNeedsReinit, this);
    if (status != noErr) {
        CA_LogError(status, "Failed to add listener for kAudioDevicePropertyIOStoppedAbnormally");
        return false;
    }

    return true;
}

void CoreAudioRenderer::deinitListeners()
{
    AudioObjectPropertyAddress addr{kAudioDeviceProcessorOverload, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
    AudioObjectRemovePropertyListener(m_OutputDeviceID, &addr, onDeviceOverload, this);

    addr.mSelector = kAudioDevicePropertyDeviceHasChanged;
    AudioObjectRemovePropertyListener(m_OutputDeviceID, &addr, onAudioNeedsReinit, this);

    addr.mSelector = kAudioHardwarePropertyServiceRestarted;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr, onAudioNeedsReinit, this);

    addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &addr, onAudioNeedsReinit, this);
}

bool CoreAudioRenderer::setCallback(AURenderCallback callback)
{
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = callback;
    callbackStruct.inputProcRefCon = this;

    OSStatus status = AudioUnitSetProperty(m_OutputAU, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Output, 0, &callbackStruct, sizeof(callbackStruct));
    if (status != noErr) {
        CA_LogError(status, "Failed to set output render callback");
        return false;
    }

    return true;
}

void* CoreAudioRenderer::getAudioBuffer(int* size)
{
    // We must always write a full frame of audio. If we don't,
    // the reader will get out of sync with the writer and our
    // channels will get all mixed up. To ensure this is always
    // the case, round our bytes free down to the next multiple
    // of our frame size.
    uint32_t bytesFree;
    void *ptr = TPCircularBufferHead(&m_RingBuffer, &bytesFree);
    int bytesPerFrame = m_opusConfig->channelCount * sizeof(float);
    *size = qMin(*size, (int)(bytesFree / bytesPerFrame) * bytesPerFrame);

    m_BufferFilledBytes = m_RingBuffer.length - bytesFree;

    return ptr;
}

bool CoreAudioRenderer::submitAudio(int bytesWritten)
{
    // We'll be fully recreated after any changes to the audio device, default output, etc.
    if (m_needsReinit) {
        return false;
    }

    if (bytesWritten == 0) {
        // Nothing to do
        return true;
    }

    // drop packet if we've fallen behind Moonlight's queue by at least 30 ms
    if (LiGetPendingAudioDuration() > 30) {
        m_ActiveWndAudioStats.totalGlitches++;
        m_ActiveWndAudioStats.droppedOverload++;
        return true;
    }

    // Advance the write pointer
    TPCircularBufferProduce(&m_RingBuffer, bytesWritten);

    return true;
}

AUSpatialMixerOutputType CoreAudioRenderer::getSpatialMixerOutputType()
{
#if TARGET_OS_OSX
    // Check if headphones are plugged in.
    uint32_t dataSource{};
    uint32_t size = sizeof(dataSource);

    AudioObjectPropertyAddress addTransType{kAudioDevicePropertyTransportType, kAudioObjectPropertyScopeOutput, kAudioObjectPropertyElementMain};
    OSStatus status = AudioObjectGetPropertyData(m_OutputDeviceID, &addTransType, 0, nullptr, &size, &dataSource);
    if (status != noErr) {
        CA_LogError(status, "Failed to get the transport type of output device");
        return kSpatialMixerOutputType_ExternalSpeakers;
    }

    CA_FourCC(dataSource, m_OutputTransportType);
    DEBUG_TRACE("CoreAudioRenderer output transport type %s", m_OutputTransportType);

    if (dataSource == kAudioDeviceTransportTypeHDMI) {
        dataSource = kIOAudioOutputPortSubTypeExternalSpeaker;
    } else if (dataSource == kAudioDeviceTransportTypeBluetooth || dataSource == kAudioDeviceTransportTypeUSB) {
        dataSource = kIOAudioOutputPortSubTypeHeadphones;
    } else {
        AudioObjectPropertyAddress theAddress{kAudioDevicePropertyDataSource, kAudioDevicePropertyScopeOutput, kAudioObjectPropertyElementMain};

        status = AudioObjectGetPropertyData(m_OutputDeviceID, &theAddress, 0, nullptr, &size, &dataSource);
        if (status != noErr) {
            CA_LogError(status, "Couldn't determine default audio device type, defaulting to ExternalSpeakers");
            return kSpatialMixerOutputType_ExternalSpeakers;
        }
    }

    CA_FourCC(dataSource, m_OutputDataSource);
    DEBUG_TRACE("CoreAudioRenderer output data source %s", m_OutputDataSource);

    switch (dataSource) {
        case kIOAudioOutputPortSubTypeInternalSpeaker:
            return kSpatialMixerOutputType_BuiltInSpeakers;
            break;

        case kIOAudioOutputPortSubTypeHeadphones:
            return kSpatialMixerOutputType_Headphones;
            break;

        case kIOAudioOutputPortSubTypeExternalSpeaker:
            return kSpatialMixerOutputType_ExternalSpeakers;
            break;

        default:
            return kSpatialMixerOutputType_Headphones;
            break;
    }
#else
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];

    if ([audioSession.currentRoute.outputs count] != 1) {
        return kSpatialMixerOutputType_ExternalSpeakers;
    } else {
        NSString* pType = audioSession.currentRoute.outputs.firstObject.portType;
        if ([pType isEqualToString:AVAudioSessionPortHeadphones] || [pType isEqualToString:AVAudioSessionPortBluetoothA2DP] || [pType isEqualToString:AVAudioSessionPortBluetoothLE] || [pType isEqualToString:AVAudioSessionPortBluetoothHFP]) {
            return kSpatialMixerOutputType_Headphones;
        } else if ([pType isEqualToString:AVAudioSessionPortBuiltInSpeaker]) {
            return kSpatialMixerOutputType_BuiltInSpeakers;
        } else {
            return kSpatialMixerOutputType_ExternalSpeakers;
        }
    }
#endif
}

static void replace_fancy_quote(char *str)
{
    char *pos;
    while ((pos = strstr(str, "\xe2\x80\x99")) != NULL) {
        *pos = '\'';
        memmove(pos + 1, pos + 3, strlen(pos + 3) + 1);
    }
}

void CoreAudioRenderer::setOutputDeviceName(const CFStringRef cfstr)
{
    if (cfstr) {
        CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
        char *buffer = (char *)malloc(size);
        CFStringGetCString(cfstr, buffer, size, kCFStringEncodingUTF8);

        // it's very likely we'll get a name like "Andy’s AirPods Pro"
        // with a UTF8 quote, and our overlay font is only ASCII
        replace_fancy_quote(buffer);

        if (m_OutputDeviceName) {
            free(m_OutputDeviceName);
        }

        m_OutputDeviceName = buffer;
    }
}
