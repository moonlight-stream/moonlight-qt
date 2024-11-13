#import "au_spatial_renderer.h"
#import "coreaudio_helpers.h"
#import "AllocatedAudioBufferList.h"
#include "settings/streamingpreferences.h"

#import <Accelerate/Accelerate.h>
#import <AVFoundation/AVFoundation.h>
#import <QtGlobal>

AUSpatialRenderer::AUSpatialRenderer()
    : m_HeadTracking(0),
      m_PersonalizedHRTF(0),
      m_AudioUnitLatency(0.0)
{
    DEBUG_TRACE("AUSpatialRenderer construct");

    AudioComponentDescription desc = {kAudioUnitType_Mixer,
                                      kAudioUnitSubType_SpatialMixer,
                                      kAudioUnitManufacturer_Apple,
                                      0,
                                      0};
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    assert(comp);

    OSStatus status = AudioComponentInstanceNew(comp, &m_Mixer);
    if (status != noErr) {
        CA_LogError(status, "Failed to create Spatial Mixer");
        assert(status == noErr);
    }
}

AUSpatialRenderer::~AUSpatialRenderer()
{
    DEBUG_TRACE("AUSpatialRenderer destruct");

    if (m_Mixer) {
        AudioComponentInstanceDispose(m_Mixer);
    }
}

double AUSpatialRenderer::getAudioUnitLatency()
{
    return m_AudioUnitLatency;
}

void AUSpatialRenderer::setRingBufferPtr(const TPCircularBuffer *buffer)
{
    m_RingBufferPtr = buffer;
}

OSStatus AUSpatialRenderer::setStreamFormatAndACL(float inSampleRate,
                                                  AudioChannelLayoutTag inLayoutTag,
                                                  AudioUnitScope inScope,
                                                  AudioUnitElement inElement)
{
    AVAudioChannelLayout* layout = [AVAudioChannelLayout layoutWithLayoutTag:inLayoutTag];
    AVAudioFormat *format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                   sampleRate:inSampleRate
                                                   interleaved:NO
                                                   channelLayout:layout];

    const AudioStreamBasicDescription* asbd = [format streamDescription];
    if (inScope == kAudioUnitScope_Input) {
        CA_PrintASBD("CoreAudioRenderer spatial mixer input AudioStreamBasicDescription:", asbd);
    } else {
        CA_PrintASBD("CoreAudioRenderer spatial mixer output AudioStreamBasicDescription:", asbd);
    }
    OSStatus status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_StreamFormat, inScope, inElement, asbd, sizeof(AudioStreamBasicDescription));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer StreamFormat scope=%d", inScope);
        return status;
    }

    const AudioChannelLayout* outLayout = [layout layout];
    status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_AudioChannelLayout, inScope, inElement, outLayout, sizeof(AudioChannelLayout));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer AudioChannelLayout scope=%d, layout=%d", inScope, outLayout);
        return status;
    }

    return noErr;
}

OSStatus AUSpatialRenderer::setOutputType(AUSpatialMixerOutputType outputType)
{
    return AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SpatialMixerOutputType, kAudioUnitScope_Global, 0, &outputType, sizeof(outputType));
}

// realtime method
OSStatus inputCallback(void *inRefCon,
                       AudioUnitRenderActionFlags *ioActionFlags,
                       const AudioTimeStamp * /*inTimestamp*/,
                       uint32_t /*inBusNumber*/,
                       uint32_t inNumberFrames,
                       AudioBufferList *ioData)
{
    AUSpatialRenderer *me = (AUSpatialRenderer *)inRefCon;

    // Clear the buffer
    for (uint32_t i = 0; i < ioData->mNumberBuffers; i++) {
        // faster version of memset((float *)ioData->mBuffers[i].mData, 0, inNumberFrames * 4);
        vDSP_vclr((float *)ioData->mBuffers[i].mData, 1, inNumberFrames * 4);
    }

    // Pull audio from playthrough buffer
    uint32_t availableBytes;
    float *ringBuffer = (float *)TPCircularBufferTail((TPCircularBuffer *)me->m_RingBufferPtr, &availableBytes);

    // Total size of interleaved PCM for all channels
    uint32_t channelCount = ioData->mNumberBuffers;
    uint32_t wantedBytes  = channelCount * inNumberFrames * 4;

    if (availableBytes < wantedBytes) {
        // not enough data for all channels, note we are sending back a zeroed-out buffer
        // This underrun is not always a problem, so it's not included in stats currently
        // XXX this ioActionFlags with silence flag seems to get lost, instead of returning via
        // AudioUnitRender() <- process() <- renderSpatialCallback()
        *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;
        return noErr;
    }

    // de-interleave ringBuffer PCM data into per-channel buffers
    const float zero = 0.0f;
    for (uint32_t channel = 0; channel < channelCount; channel++) {
        float *channelBuffer = (float *)ioData->mBuffers[channel].mData;
        vDSP_vsadd(ringBuffer + channel, channelCount, &zero, channelBuffer, 1, inNumberFrames);
    }

    TPCircularBufferConsume((TPCircularBuffer *)me->m_RingBufferPtr, wantedBytes);

    // The Apple example included this but it doesn't seem to do anything?
    // (*ioActionFlags) = kAudioOfflineUnitRenderAction_Complete;

    return noErr;
}

bool AUSpatialRenderer::setup(AUSpatialMixerOutputType outputType, float sampleRate, int inChannelCount)
{
     // Set the number of input elements (buses).
    uint32_t numInputs = 1;
    OSStatus status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &numInputs, sizeof(numInputs));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer numInputs to 1");
        return false;
    }

    // Set up the output stream format and channel layout for stereo.
    status = setStreamFormatAndACL(sampleRate, kAudioChannelLayoutTag_Stereo, kAudioUnitScope_Output, 0);
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer output stream format to stereo");
        return false;
    }

    // Set up the input stream format as multichannel with 5.1 or 7.1 channel layout
    // If it's ever possible to access a 12-channel Atmos bitstream from Windows, it should work here too
    AudioChannelLayoutTag layout;
    switch (inChannelCount) {
        case 2:
            layout = kAudioChannelLayoutTag_Stereo;
            break;
        case 6:
            // Back in the DVD era I remember 5.1 meant side surrounds (WAVE_5_1_A), but at some point it became back surrounds?
            // layout = kAudioChannelLayoutTag_WAVE_5_1_A; // L R C LFE Ls Rs
            layout = kAudioChannelLayoutTag_WAVE_5_1_B; // L R C LFE Rls Rrs
            break;
        case 8:
            layout = kAudioChannelLayoutTag_WAVE_7_1; // L R C LFE Rls Rrs Ls Rs
            break;
        case 12:
            layout = kAudioChannelLayoutTag_Atmos_7_1_4; // L R C LFE Ls Rs Rls Rrs Vhl Vhr Ltr Rtr
            break;
        default:
            CA_LogError(-1, "Unsupported number of channels for spatial audio mixer: %d", inChannelCount);
            return false;
    }

    // XXX Allow user to override channel layout

    status = setStreamFormatAndACL(sampleRate, layout, kAudioUnitScope_Input, 0);
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer input stream format to %d channels", inChannelCount);
        return false;
    }

    // Apple docs say: Use kSpatializationAlgorithm_UseOutputType with appropriate kAudioUnitProperty_SpatialMixerOutputType
    // for highest-quality spatial rendering across different hardware.
    uint32_t renderingAlgorithm = kSpatializationAlgorithm_UseOutputType;
    DEBUG_TRACE("AUSpatialRenderer kAudioUnitProperty_SpatializationAlgorithm set to UseOutputType (%d)", renderingAlgorithm);
    status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 0, &renderingAlgorithm, sizeof(renderingAlgorithm));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer spatialization algorithm");
        return false;
    }

    // Set the source mode. AmbienceBed causes the input channels to be spatialized around the listener as far-field sources.
    uint32_t sourceMode = kSpatialMixerSourceMode_AmbienceBed;
    DEBUG_TRACE("AUSpatialRenderer kAudioUnitProperty_SpatialMixerSourceMode set to AmbienceBed (%d)", sourceMode);
    status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SpatialMixerSourceMode, kAudioUnitScope_Input, 0, &sourceMode, sizeof(sourceMode));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer source mode");
        return false;
    }

    // Set up the output type to adapt the rendering depending on the physical output.
    // The unit renders binaural for headphones, Apple-proprietary for built-in
    // speakers, or multichannel for external speakers.
    DEBUG_TRACE("AUSpatialRenderer setOutputType %d", outputType);
    status = setOutputType(outputType);
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer output type");
        return false;
    }

#if TARGET_OS_OSX
    if (@available(macOS 13.0, *))
#elif TARGET_OS_IOS
    if (@available(iOS 18.0, *))
#elif TARGET_OS_TV
    if (@available(tvOS 18.0, *))
#endif
    {
        if (outputType == kSpatialMixerOutputType_Headphones) {
            // XXX: Both of these might require the builder to have a paid Apple developer account due to the use of entitlements.

            // For devices that support it, enable head-tracking.
            // Apps that use low-latency head-tracking in iOS/tvOS need to set
            // the audio session category to ambient or run in Game Mode.
            // Head tracking requires the entitlement com.apple.developer.coremotion.head-pose.

            // XXX Head-tracking may cause audio glitches. It's off by default.
            StreamingPreferences *prefs = StreamingPreferences::get();
            if (prefs->spatialHeadTracking) {
                uint32_t ht = 1;
                status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SpatialMixerEnableHeadTracking, kAudioUnitScope_Global, 0, &ht, sizeof(uint32_t));
                if (status != noErr) {
                    CA_LogError(status, "Failed to enable head tracking");
                }
                else {
                    DEBUG_TRACE("AUSpatialRenderer enabled head-tracking");
                    m_HeadTracking = 1;
                }
            }

            // For devices that support it, enable personalized head-related transfer function (HRTF).
            // HRTF requires the entitlement com.apple.developer.spatial-audio.profile-access.
            // https://developer.apple.com/documentation/bundleresources/entitlements/com_apple_developer_spatial-audio_profile-access
            // This is an opportunistic API, so if personalized HRTF isn't available, the
            // system falls back to generic HRTF.
            uint32_t hrtf = kSpatialMixerPersonalizedHRTFMode_Auto;
            status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SpatialMixerPersonalizedHRTFMode, kAudioUnitScope_Global, 0, &hrtf, sizeof(uint32_t));
            if (status != noErr) {
                CA_LogError(status, "Failed to enable personalized spatial audio");
            }
            else {
                DEBUG_TRACE("AUSpatialRenderer set personalized HRTF mode to auto");
            }
        }
    }

#if TARGET_OS_IOS
    if (@available(iOS 18.0, *))
#elif TARGET_OS_TV
    if (@available(tvOS 18.0, *))
#endif
    {
        // Set a factory preset to use with media playback on an Apple device.
        // This can override previously set properties. Check the available
        // presets by using `auval` command. For example, `auval -v aumx 3dem appl`
        // may list the following presets:
        //
        // ID:   0    Name: Built-In Speaker Media Playback
        // ID:   1    Name: Headphone Media Playback Default
        // ID:   2    Name: Headphone Media Playback Movie
        AUPreset preset {
            outputType == kSpatialMixerOutputType_BuiltInSpeakers ? 0 : 1,
            NULL
        };
        status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_PresentPreset, kAudioUnitScope_Global, 0, &preset, sizeof(AUPreset));
        if (status != noErr) {
            CA_LogError(status, "Failed to set AUSpatialRenderer factory preset");
            return false;
        }
    }


    // Set the maximum frames we can process per callback (must match size of m_SpatialBuffer)
    uint32_t mfps = 4096;
    status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mfps, sizeof(mfps));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer max frame size");
        return false;
    }

    // Set up the input callback that pulls n channels of PCM from the ringBuffer
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = inputCallback;
    callbackStruct.inputProcRefCon = this;
    DEBUG_TRACE("AUSpatialRenderer set input callback");
    status = AudioUnitSetProperty(m_Mixer, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callbackStruct, sizeof(callbackStruct));
    if (status != noErr) {
        CA_LogError(status, "Failed to set AUSpatialRenderer input callback");
        return false;
    }

    // We're ready!
    DEBUG_TRACE("AUSpatialRenderer initialize");
    status = AudioUnitInitialize(m_Mixer);
    if (status != noErr) {
        CA_LogError(status, "Failed to initialize AUSpatialRenderer");
        return false;
    }

#if TARGET_OS_OSX
    // you can set HRTF in 13 but only check the status in 14
    if (@available(macOS 14.0, *))
#elif TARGET_OS_IOS
    if (@available(iOS 18.0, *))
#elif TARGET_OS_TV
    if (@available(tvOS 18.0, *))
#endif
    {
        // After initialize, we can check if personalized HRTF is actually being used
        if (outputType == kSpatialMixerOutputType_Headphones) {
            m_PersonalizedHRTF = 0;
            uint32_t size = sizeof(m_PersonalizedHRTF);
            status = AudioUnitGetProperty(m_Mixer, kAudioUnitProperty_SpatialMixerAnyInputIsUsingPersonalizedHRTF, kAudioUnitScope_Global, 0, &m_PersonalizedHRTF, &size);
            if (status != noErr) {
                CA_LogError(status, "Failed to get AUSpatialRenderer personalized HRTF status");
            }
            else {
                DEBUG_TRACE("AUSpatialRenderer actual personalized HRTF status: %s", m_PersonalizedHRTF ? "enabled" : "disabled");
            }
        }
    }

    // Get the internal AudioUnit latency (processing time from input to output)
    {
        m_AudioUnitLatency = 0.0;
        uint32_t size = sizeof(m_AudioUnitLatency);
        status = AudioUnitGetProperty(m_Mixer, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &m_AudioUnitLatency, &size);
        if (status != noErr) {
            CA_LogError(status, "Failed to get SpatialAU AudioUnit latency");
            return false;
        }
        DEBUG_TRACE("CoreAudioRenderer SpatialAU AudioUnit latency: %0.2f ms", m_AudioUnitLatency * 1000.0);
    }

    return true;
}

// realtime method
OSStatus AUSpatialRenderer::process(AudioBufferList* __nullable outputABL,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp* __nullable inTimestamp,
                                float inNumberFrames)
{
    return AudioUnitRender(m_Mixer, ioActionFlags, inTimestamp, 0, inNumberFrames, outputABL);
}
