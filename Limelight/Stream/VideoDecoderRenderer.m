//
//  VideoDecoderRenderer.m
//  Moonlight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Moonlight Stream. All rights reserved.
//

#import "VideoDecoderRenderer.h"
#import "StreamView.h"

#include "Limelight.h"

@implementation VideoDecoderRenderer {
    StreamView* _view;
    
    AVSampleBufferDisplayLayer* displayLayer;
    Boolean waitingForSps, waitingForPps, waitingForVps;
    int videoFormat;
    
    NSData *spsData, *ppsData, *vpsData;
    CMVideoFormatDescriptionRef formatDesc;
}

- (void)reinitializeDisplayLayer
{
    CALayer *oldLayer = displayLayer;
    
    displayLayer = [[AVSampleBufferDisplayLayer alloc] init];
    displayLayer.bounds = _view.bounds;
    displayLayer.backgroundColor = [OSColor blackColor].CGColor;
    
    displayLayer.position = CGPointMake(CGRectGetMidX(_view.bounds), CGRectGetMidY(_view.bounds));
    displayLayer.videoGravity = AVLayerVideoGravityResizeAspect;
    
    if (oldLayer != nil) {
        // Switch out the old display layer with the new one
        [_view.layer replaceSublayer:oldLayer with:displayLayer];
    }
    else {
        [_view.layer addSublayer:displayLayer];
    }
    
    // We need some parameter sets before we can properly start decoding frames
    waitingForSps = true;
    spsData = nil;
    waitingForPps = true;
    ppsData = nil;
    waitingForVps = true;
    vpsData = nil;
    
    if (formatDesc != nil) {
        CFRelease(formatDesc);
        formatDesc = nil;
    }
}

- (id)initWithView:(StreamView*)view
{
    self = [super init];
    
    _view = view;
    
    [self reinitializeDisplayLayer];
    
    return self;
}

- (void)setupWithVideoFormat:(int)videoFormat
{
    self->videoFormat = videoFormat;
#if !TARGET_OS_IPHONE
    _view.codec = videoFormat;
#endif
}

#define FRAME_START_PREFIX_SIZE 4
#define NALU_START_PREFIX_SIZE 3
#define NAL_LENGTH_PREFIX_SIZE 4

- (Boolean)readyForPictureData
{
    if (videoFormat & VIDEO_FORMAT_MASK_H264) {
        return !waitingForSps && !waitingForPps;
    }
    else {
        // H.265 requires VPS in addition to SPS and PPS
        return !waitingForVps && !waitingForSps && !waitingForPps;
    }
}

- (Boolean)isNalReferencePicture:(unsigned char)nalType
{
    if (videoFormat & VIDEO_FORMAT_MASK_H264) {
        return nalType == 0x65;
    }
    else {
        // HEVC has several types of reference NALU types
        switch (nalType) {
            case 0x20:
            case 0x22:
            case 0x24:
            case 0x26:
            case 0x28:
            case 0x2A:
                return true;
            default:
                return false;
        }
    }
}

- (void)updateBufferForRange:(CMBlockBufferRef)existingBuffer data:(unsigned char *)data offset:(int)offset length:(int)nalLength
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
            Log(LOG_E, @"CMBlockBufferReplaceDataBytes failed: %d", (int)status);
            return;
        }
        
        // Write the length prefix to existing buffer
        const uint8_t lengthBytes[] = {(uint8_t)(dataLength >> 24), (uint8_t)(dataLength >> 16),
            (uint8_t)(dataLength >> 8), (uint8_t)dataLength};
        status = CMBlockBufferReplaceDataBytes(lengthBytes, existingBuffer,
                                               oldOffset, NAL_LENGTH_PREFIX_SIZE);
        if (status != noErr) {
            Log(LOG_E, @"CMBlockBufferReplaceDataBytes failed: %d", (int)status);
            return;
        }
    } else {
        // Append a 4 byte buffer to this block for the length prefix
        status = CMBlockBufferAppendMemoryBlock(existingBuffer, NULL,
                                                NAL_LENGTH_PREFIX_SIZE,
                                                kCFAllocatorDefault, NULL, 0,
                                                NAL_LENGTH_PREFIX_SIZE, 0);
        if (status != noErr) {
            Log(LOG_E, @"CMBlockBufferAppendMemoryBlock failed: %d", (int)status);
            return;
        }
        
        // Write the length prefix to the new buffer
        int dataLength = nalLength - NALU_START_PREFIX_SIZE;
        const uint8_t lengthBytes[] = {(uint8_t)(dataLength >> 24), (uint8_t)(dataLength >> 16),
            (uint8_t)(dataLength >> 8), (uint8_t)dataLength};
        status = CMBlockBufferReplaceDataBytes(lengthBytes, existingBuffer,
                                               oldOffset, NAL_LENGTH_PREFIX_SIZE);
        if (status != noErr) {
            Log(LOG_E, @"CMBlockBufferReplaceDataBytes failed: %d", (int)status);
            return;
        }
        
        // Attach the buffer by reference to the block buffer
        status = CMBlockBufferAppendMemoryBlock(existingBuffer, &data[offset+NALU_START_PREFIX_SIZE],
                                                dataLength,
                                                kCFAllocatorNull, // Don't deallocate data on free
                                                NULL, 0, dataLength, 0);
        if (status != noErr) {
            Log(LOG_E, @"CMBlockBufferReplaceDataBytes failed: %d", (int)status);
            return;
        }
    }
}

// This function must free data for bufferType == BUFFER_TYPE_PICDATA
- (int)submitDecodeBuffer:(unsigned char *)data length:(int)length bufferType:(int)bufferType
{
    unsigned char nalType = data[FRAME_START_PREFIX_SIZE];
    OSStatus status;
    
    if (bufferType != BUFFER_TYPE_PICDATA) {
        if (bufferType == BUFFER_TYPE_VPS) {
            Log(LOG_I, @"Got VPS");
            vpsData = [NSData dataWithBytes:&data[FRAME_START_PREFIX_SIZE] length:length - FRAME_START_PREFIX_SIZE];
            waitingForVps = false;
            
            // We got a new VPS so wait for a new SPS to match it
            waitingForSps = true;
        }
        else if (bufferType == BUFFER_TYPE_SPS) {
            Log(LOG_I, @"Got SPS");
            spsData = [NSData dataWithBytes:&data[FRAME_START_PREFIX_SIZE] length:length - FRAME_START_PREFIX_SIZE];
            waitingForSps = false;
            
            // We got a new SPS so wait for a new PPS to match it
            waitingForPps = true;
        } else if (bufferType == BUFFER_TYPE_PPS) {
            Log(LOG_I, @"Got PPS");
            ppsData = [NSData dataWithBytes:&data[FRAME_START_PREFIX_SIZE] length:length - FRAME_START_PREFIX_SIZE];
            waitingForPps = false;
        }
        
        // See if we've got all the parameter sets we need for our video format
        if ([self readyForPictureData]) {
            if (videoFormat & VIDEO_FORMAT_MASK_H264) {
                const uint8_t* const parameterSetPointers[] = { [spsData bytes], [ppsData bytes] };
                const size_t parameterSetSizes[] = { [spsData length], [ppsData length] };
                
                Log(LOG_I, @"Constructing new H264 format description");
                status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                             2, /* count of parameter sets */
                                                                             parameterSetPointers,
                                                                             parameterSetSizes,
                                                                             NAL_LENGTH_PREFIX_SIZE,
                                                                             &formatDesc);
                if (status != noErr) {
                    Log(LOG_E, @"Failed to create H264 format description: %d", (int)status);
                    formatDesc = NULL;
                }
            }
            else {
                const uint8_t* const parameterSetPointers[] = { [vpsData bytes], [spsData bytes], [ppsData bytes] };
                const size_t parameterSetSizes[] = { [vpsData length], [spsData length], [ppsData length] };
                
                Log(LOG_I, @"Constructing new HEVC format description");

#if TARGET_OS_IPHONE
                if (@available(iOS 11.0, *)) {
#else
                if (@available(macOS 10.13, *)) {
#endif
                    status = CMVideoFormatDescriptionCreateFromHEVCParameterSets(kCFAllocatorDefault,
                                                                                 3, /* count of parameter sets */
                                                                                 parameterSetPointers,
                                                                                 parameterSetSizes,
                                                                                 NAL_LENGTH_PREFIX_SIZE,
                                                                                 nil,
                                                                                 &formatDesc);
                } else {
                    // This means Moonlight-common-c decided to give us an HEVC stream
                    // even though we said we couldn't support it. All we can do is abort().
                    abort();
                }
                
                if (status != noErr) {
                    Log(LOG_E, @"Failed to create HEVC format description: %d", (int)status);
                    formatDesc = NULL;
                }
            }
        }
        
        // Data is NOT to be freed here. It's a direct usage of the caller's buffer.
        
        // No frame data to submit for these NALUs
        return DR_OK;
    }
    
    if (formatDesc == NULL) {
        // Can't decode if we haven't gotten our parameter sets yet
        free(data);
        return DR_NEED_IDR;
    }
    
    // Check for previous decoder errors before doing anything
    if (displayLayer.status == AVQueuedSampleBufferRenderingStatusFailed) {
        Log(LOG_E, @"Display layer rendering failed: %@", displayLayer.error);
        
        // Recreate the display layer
        [self reinitializeDisplayLayer];
        
        // Request an IDR frame to initialize the new decoder
        free(data);
        return DR_NEED_IDR;
    }
    
    // Now we're decoding actual frame data here
    CMBlockBufferRef blockBuffer;
    
    status = CMBlockBufferCreateEmpty(NULL, 0, 0, &blockBuffer);
    if (status != noErr) {
        Log(LOG_E, @"CMBlockBufferCreateEmpty failed: %d", (int)status);
        free(data);
        return DR_NEED_IDR;
    }
    
    int lastOffset = -1;
    for (int i = 0; i < length - FRAME_START_PREFIX_SIZE; i++) {
        // Search for a NALU
        if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1) {
            // It's the start of a new NALU
            if (lastOffset != -1) {
                // We've seen a start before this so enqueue that NALU
                [self updateBufferForRange:blockBuffer data:data offset:lastOffset length:i - lastOffset];
            }
            
            lastOffset = i;
        }
    }
    
    if (lastOffset != -1) {
        // Enqueue the remaining data
        [self updateBufferForRange:blockBuffer data:data offset:lastOffset length:length - lastOffset];
    }
    
    // From now on, CMBlockBuffer owns the data pointer and will free it when it's dereferenced
    
    CMSampleBufferRef sampleBuffer;
    
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  blockBuffer,
                                  true, NULL,
                                  NULL, formatDesc, 1, 0,
                                  NULL, 0, NULL,
                                  &sampleBuffer);
    if (status != noErr) {
        Log(LOG_E, @"CMSampleBufferCreate failed: %d", (int)status);
        CFRelease(blockBuffer);
        return DR_NEED_IDR;
    }
    
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, YES);
    CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
    
    CFDictionarySetValue(dict, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    CFDictionarySetValue(dict, kCMSampleAttachmentKey_IsDependedOnByOthers, kCFBooleanTrue);
    
    if (![self isNalReferencePicture:nalType]) {
        // P-frame
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanTrue);
    } else {
        // I-frame
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanFalse);
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanFalse);
    }
    
    [displayLayer enqueueSampleBuffer:sampleBuffer];
#if !TARGET_OS_IPHONE
    _view.frameCount++;
#endif
    // Dereference the buffers
    CFRelease(blockBuffer);
    CFRelease(sampleBuffer);
    
    return DR_OK;
}

@end
