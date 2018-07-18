#include "avfoundation.h"

#include <SDL_syswm.h>

#import <Cocoa/Cocoa.h>
#import <VideoToolbox/VideoToolbox.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>

#define FRAME_START_PREFIX_SIZE 4
#define NALU_START_PREFIX_SIZE 3
#define NAL_LENGTH_PREFIX_SIZE 4

#define FUTURE_FRAMES_IN_VSYNC 3

class AVFoundationDecoder : public IVideoDecoder
{
public:
    AVFoundationDecoder()
        : m_DisplayLayer(nullptr),
          m_FormatDesc(nullptr),
          m_SpsData(nullptr),
          m_PpsData(nullptr),
          m_VpsData(nullptr),
          m_View(nullptr),
          m_NeedsIdr(false),
          m_DisplayLink(nullptr),
          m_VsyncTime(0),
          m_LastVsyncTime(0),
          m_VsyncStatsLock(0)
    {
        SDL_zero(m_FrameCountOnVsync);
    }

    virtual ~AVFoundationDecoder()
    {
        if (m_FormatDesc != nullptr) {
            CFRelease(m_FormatDesc);
        }

        if (m_DisplayLink != nullptr) {
            CVDisplayLinkStop(m_DisplayLink);
            CVDisplayLinkRelease(m_DisplayLink);
        }

        delete m_SpsData;
        delete m_PpsData;
        delete m_VpsData;
    }

    static
    CVReturn
    displayLinkOutputCallback(
        CVDisplayLinkRef,
        const CVTimeStamp*,
        const CVTimeStamp* vsyncTime,
        CVOptionFlags,
        CVOptionFlags*,
        void *displayLinkContext)
    {
        AVFoundationDecoder* me = reinterpret_cast<AVFoundationDecoder*>(displayLinkContext);

        SDL_AtomicLock(&me->m_VsyncStatsLock);
        me->m_LastVsyncTime = me->m_VsyncTime;
        me->m_VsyncTime = vsyncTime->hostTime;

        int framesThisVsync = *(volatile int*)&me->m_FrameCountOnVsync[0];

        memmove(&me->m_FrameCountOnVsync[0],
                &me->m_FrameCountOnVsync[1],
                sizeof(me->m_FrameCountOnVsync) - sizeof(me->m_FrameCountOnVsync[0]));
        me->m_FrameCountOnVsync[FUTURE_FRAMES_IN_VSYNC - 1] = 0;
        SDL_AtomicUnlock(&me->m_VsyncStatsLock);

        if (framesThisVsync != 1)
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "V-Sync interval frames: %d",
                        framesThisVsync);

        return kCVReturnSuccess;
    }

    virtual bool initialize(StreamingPreferences::VideoDecoderSelection vds,
                            SDL_Window* window,
                            int videoFormat,
                            int, int, int) override
    {
        m_VideoFormat = videoFormat;

        // Fail if the user wants software decode only
        if (vds == StreamingPreferences::VDS_FORCE_SOFTWARE) {
            return false;
        }

        if (videoFormat & VIDEO_FORMAT_MASK_H264) {
            // Prior to 10.13, we'll just assume everything has
            // H.264 support and fail open to allow VT decode.
    #if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
            if (__builtin_available(macOS 10.13, *)) {
                if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_H264)) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "No HW accelerated H.264 decode via VT");
                    return false;
                }
            }
            else
    #endif
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Assuming H.264 HW decode on < macOS 10.13");
            }
        }
        else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
    #if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
            if (__builtin_available(macOS 10.13, *)) {
                if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC)) {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "No HW accelerated HEVC decode via VT");
                    return false;
                }
            }
            else
    #endif
            {
                // Fail closed for HEVC if we're not on 10.13+
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HEVC support on < macOS 10.13");
                return false;
            }
        }

        SDL_SysWMinfo info;

        SDL_VERSION(&info.version);

        if (!SDL_GetWindowWMInfo(window, &info)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "SDL_GetWindowWMInfo() failed: %s",
                        SDL_GetError());
            return false;
        }

        SDL_assert(info.subsystem == SDL_SYSWM_COCOA);

        // SDL adds its own content view to listen for events.
        // We need to add a subview for our display layer.
        NSView* contentView = info.info.cocoa.window.contentView;
        m_View = [[NSView alloc] initWithFrame:contentView.bounds];
        m_View.wantsLayer = YES;
        [contentView addSubview: m_View];

        setupDisplayLayer();

        CVDisplayLinkCreateWithActiveCGDisplays(&m_DisplayLink);
        CVDisplayLinkSetOutputCallback(m_DisplayLink, displayLinkOutputCallback, (void*)this);
        CVDisplayLinkStart(m_DisplayLink);

        return true;
    }

    virtual bool isHardwareAccelerated() override
    {
        // We only use AVFoundation for HW acceleration
        return true;
    }

    virtual int submitDecodeUnit(PDECODE_UNIT du) override
    {
        OSStatus status;

        unsigned char* data = (unsigned char*)malloc(du->fullLength);
        if (!data) {
            return DR_NEED_IDR;
        }

        PLENTRY entry = du->bufferList;
        int pictureDataLength = 0;
        while (entry != nullptr) {
            if (entry->bufferType == BUFFER_TYPE_VPS) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Got VPS");
                delete m_VpsData;
                m_VpsData = new QByteArray(&entry->data[FRAME_START_PREFIX_SIZE], entry->length - FRAME_START_PREFIX_SIZE);

                // We got a new VPS so wait for a new SPS to match it
                delete m_SpsData;
                delete m_PpsData;
                m_SpsData = m_PpsData = nullptr;
            }
            else if (entry->bufferType == BUFFER_TYPE_SPS) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Got SPS");
                delete m_SpsData;
                m_SpsData = new QByteArray(&entry->data[FRAME_START_PREFIX_SIZE], entry->length - FRAME_START_PREFIX_SIZE);

                // We got a new SPS so wait for a new PPS to match it
                delete m_PpsData;
                m_PpsData = nullptr;
            }
            else if (entry->bufferType == BUFFER_TYPE_PPS) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Got PPS");
                delete m_PpsData;
                m_PpsData = new QByteArray(&entry->data[FRAME_START_PREFIX_SIZE], entry->length - FRAME_START_PREFIX_SIZE);
            }
            else {
                // Picture data
                SDL_assert(entry->bufferType == BUFFER_TYPE_PICDATA);
                memcpy(&data[pictureDataLength],
                       entry->data,
                       entry->length);
                pictureDataLength += entry->length;
            }

            entry = entry->next;
        }

        SDL_assert(pictureDataLength <= du->fullLength);

        if (du->frameType == FRAME_TYPE_IDR) {
            m_NeedsIdr = false;

            if (m_FormatDesc != nullptr) {
                CFRelease(m_FormatDesc);
            }

            if (m_VideoFormat & VIDEO_FORMAT_MASK_H264) {
                const uint8_t* const parameterSetPointers[] = { (uint8_t*)m_SpsData->data(),
                                                                (uint8_t*)m_PpsData->data() };
                const size_t parameterSetSizes[] = { (size_t)m_SpsData->length(),
                                                     (size_t)m_PpsData->length() };

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Constructing new H264 format description");
                status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                             2, /* count of parameter sets */
                                                                             parameterSetPointers,
                                                                             parameterSetSizes,
                                                                             NAL_LENGTH_PREFIX_SIZE,
                                                                             &m_FormatDesc);
                if (status != noErr) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                 "Failed to create H264 format description: %d",
                                 (int)status);
                    m_FormatDesc = NULL;
                }
            }
            else {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
                const uint8_t* const parameterSetPointers[] = { (uint8_t*)m_VpsData->data(),
                                                                (uint8_t*)m_SpsData->data(),
                                                                (uint8_t*)m_PpsData->data() };
                const size_t parameterSetSizes[] = { (size_t)m_VpsData->length(),
                                                     (size_t)m_SpsData->length(),
                                                     (size_t)m_PpsData->length() };

                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Constructing new HEVC format description");

                if (__builtin_available(macOS 10.13, *)) {
                    status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault,
                                                                                 3, /* count of parameter sets */
                                                                                 parameterSetPointers,
                                                                                 parameterSetSizes,
                                                                                 NAL_LENGTH_PREFIX_SIZE,
                                                                                 nil,
                                                                                 &m_FormatDesc);
                    if (status != noErr) {
                        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                                     "Failed to create HEVC format description: %d",
                                     (int)status);
                        m_FormatDesc = NULL;
                    }
                }
                else
#endif
                {
                    // This means Moonlight-common-c decided to give us an HEVC stream
                    // even though we said we couldn't support it. All we can do is abort().
                    abort();
                }
            }
        }

        if (m_FormatDesc == nullptr || m_NeedsIdr) {
            // Can't decode if we haven't gotten our parameter sets yet
            free(data);
            return DR_NEED_IDR;
        }

        // Now we're decoding actual frame data here
        CMBlockBufferRef blockBuffer;

        status = CMBlockBufferCreateEmpty(NULL, 0, 0, &blockBuffer);
        if (status != noErr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CMBlockBufferCreateEmpty failed: %d",
                         (int)status);
            free(data);
            return DR_NEED_IDR;
        }

        int lastOffset = -1;
        for (int i = 0; i < pictureDataLength - FRAME_START_PREFIX_SIZE; i++) {
            // Search for a NALU
            if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1) {
                // It's the start of a new NALU
                if (lastOffset != -1) {
                    // We've seen a start before this so enqueue that NALU
                    updateBufferForRange(blockBuffer, data, lastOffset, i - lastOffset);
                }

                lastOffset = i;
            }
        }

        if (lastOffset != -1) {
            // Enqueue the remaining data
            updateBufferForRange(blockBuffer, data, lastOffset, pictureDataLength - lastOffset);
        }

        // From now on, CMBlockBuffer owns the data pointer and will free it when it's released

        CMSampleBufferRef sampleBuffer;

        CMSampleTimingInfo time = {
            .decodeTimeStamp = kCMTimeInvalid,
            .duration = kCMTimeInvalid
        };

        SDL_AtomicLock(&m_VsyncStatsLock);
        // Find a slot to put this frame
        for (int i = 0; i < FUTURE_FRAMES_IN_VSYNC; i++) {
            if (m_FrameCountOnVsync[i] == 0) {
                time.presentationTimeStamp =
                        CMTimeMake(m_VsyncTime + (i + 1) * (m_VsyncTime - m_LastVsyncTime),
                                   1000 * 1000 * 1000);
                m_FrameCountOnVsync[i]++;
            }
        }
        SDL_AtomicUnlock(&m_VsyncStatsLock);

        status = CMSampleBufferCreate(kCFAllocatorDefault,
                                      blockBuffer,
                                      true, NULL,
                                      NULL, m_FormatDesc, 1, 1,
                                      &time, 0, NULL,
                                      &sampleBuffer);
        if (status != noErr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "CMSampleBufferCreate failed: %d",
                         (int)status);
            CFRelease(blockBuffer);
            return DR_NEED_IDR;
        }

        // Submit for callback on the main thread
        queueFrame(sampleBuffer, (void*)(long)du->frameType);

        return DR_OK;
    }

    // This is called on the main thread, so it's safe to interact with
    // the AVSampleBufferDisplayLayer here.
    void submitBuffer(CMSampleBufferRef sampleBuffer, int frameType)
    {
        if (m_DisplayLayer.status == AVQueuedSampleBufferRenderingStatusFailed) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Resetting failed AVSampleBufferDisplay layer");
            setupDisplayLayer();
            CFRelease(sampleBuffer);
            return;
        }

        CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, YES);
        CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);

        CFDictionarySetValue(dict, kCMSampleAttachmentKey_IsDependedOnByOthers, kCFBooleanTrue);

        if (frameType == FRAME_TYPE_PFRAME) {
            // P-frame
            CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
            CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanTrue);
        } else {
            // I-frame
            CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanFalse);
            CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanFalse);
        }

        [m_DisplayLayer enqueueSampleBuffer:sampleBuffer];

        CFRelease(sampleBuffer);
    }

    virtual void renderFrame(SDL_UserEvent* event) override
    {
        submitBuffer((CMSampleBufferRef)event->data1, (int)(long)event->data2);
    }

    virtual void dropFrame(SDL_UserEvent* event) override
    {
        // Submit anyway and let the drop happen in the render pipeline
        submitBuffer((CMSampleBufferRef)event->data1, (int)(long)event->data2);
    }

private:
    void setupDisplayLayer()
    {
        CALayer* oldLayer = m_DisplayLayer;

        m_DisplayLayer = [[AVSampleBufferDisplayLayer alloc] init];
        m_DisplayLayer.bounds = m_View.bounds;
        m_DisplayLayer.position = CGPointMake(CGRectGetMidX(m_View.bounds), CGRectGetMidY(m_View.bounds));
        m_DisplayLayer.videoGravity = AVLayerVideoGravityResizeAspect;

        CALayer* viewLayer = m_View.layer;
        if (oldLayer != nil) {
            [viewLayer replaceSublayer:oldLayer with:m_DisplayLayer];
        }
        else {
            [viewLayer addSublayer:m_DisplayLayer];
        }

        // Wait for another IDR frame
        m_NeedsIdr = true;

        delete m_SpsData;
        delete m_PpsData;
        delete m_VpsData;

        m_SpsData = m_PpsData = m_VpsData = nullptr;
    }

    void updateBufferForRange(CMBlockBufferRef existingBuffer, unsigned char* data, int offset, int nalLength)
    {
        OSStatus status;
        size_t oldOffset = CMBlockBufferGetDataLength(existingBuffer);

        // If we're at index 1 (first NALU in frame), enqueue this buffer to the memory block
        // so it can handle freeing it when the block buffer is destroyed
        if (offset == 1) {
            int dataLength = nalLength - NALU_START_PREFIX_SIZE;

            // Pass the real buffer pointer directly (no offset)
            // This will give it to the block buffer to free when it's released.
            // All further calls to CMBlockBufferAppendMemoryBlock will do so
            // at an offset and will not be asking the buffer to be freed.
            status = CMBlockBufferAppendMemoryBlock(existingBuffer, data,
                                                    nalLength + 1, // Add 1 for the offset we decremented
                                                    kCFAllocatorDefault,
                                                    NULL, 0, nalLength + 1, 0);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMBlockBufferReplaceDataBytes failed: %d",
                             (int)status);
                return;
            }

            // Write the length prefix to existing buffer
            const uint8_t lengthBytes[] = {(uint8_t)(dataLength >> 24), (uint8_t)(dataLength >> 16),
                (uint8_t)(dataLength >> 8), (uint8_t)dataLength};
            status = CMBlockBufferReplaceDataBytes(lengthBytes, existingBuffer,
                                                   oldOffset, NAL_LENGTH_PREFIX_SIZE);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMBlockBufferReplaceDataBytes failed: %d",
                             (int)status);
                return;
            }
        } else {
            // Append a 4 byte buffer to this block for the length prefix
            status = CMBlockBufferAppendMemoryBlock(existingBuffer, NULL,
                                                    NAL_LENGTH_PREFIX_SIZE,
                                                    kCFAllocatorDefault, NULL, 0,
                                                    NAL_LENGTH_PREFIX_SIZE, 0);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMBlockBufferAppendMemoryBlock failed: %d",
                             (int)status);
                return;
            }

            // Write the length prefix to the new buffer
            int dataLength = nalLength - NALU_START_PREFIX_SIZE;
            const uint8_t lengthBytes[] = {(uint8_t)(dataLength >> 24), (uint8_t)(dataLength >> 16),
                (uint8_t)(dataLength >> 8), (uint8_t)dataLength};
            status = CMBlockBufferReplaceDataBytes(lengthBytes, existingBuffer,
                                                   oldOffset, NAL_LENGTH_PREFIX_SIZE);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMBlockBufferReplaceDataBytes failed: %d",
                             (int)status);
                return;
            }

            // Attach the buffer by reference to the block buffer
            status = CMBlockBufferAppendMemoryBlock(existingBuffer, &data[offset+NALU_START_PREFIX_SIZE],
                                                    dataLength,
                                                    kCFAllocatorNull, // Don't deallocate data on free
                                                    NULL, 0, dataLength, 0);
            if (status != noErr) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CMBlockBufferReplaceDataBytes failed: %d",
                             (int)status);
                return;
            }
        }
    }

    AVSampleBufferDisplayLayer* m_DisplayLayer;
    CMVideoFormatDescriptionRef m_FormatDesc;
    int m_VideoFormat;
    QByteArray* m_SpsData;
    QByteArray* m_PpsData;
    QByteArray* m_VpsData;
    NSView* m_View;
    bool m_NeedsIdr;
    CVDisplayLinkRef m_DisplayLink;
    uint64_t m_VsyncTime, m_LastVsyncTime;
    int m_FrameCountOnVsync[FUTURE_FRAMES_IN_VSYNC];
    SDL_SpinLock m_VsyncStatsLock;
};

IVideoDecoder* AVFoundationDecoderFactory::createDecoder() {
    return new AVFoundationDecoder();
}
