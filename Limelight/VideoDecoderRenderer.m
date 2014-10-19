//
//  VideoDecoderRenderer.m
//  Limelight
//
//  Created by Cameron Gutman on 10/18/14.
//  Copyright (c) 2014 Limelight Stream. All rights reserved.
//

#import "VideoDecoderRenderer.h"

@implementation VideoDecoderRenderer {
    AVSampleBufferDisplayLayer* displayLayer;
    Boolean waitingForSps, waitingForPpsA, waitingForPpsB;
    
    NSData *spsData, *ppsDataA, *ppsDataB;
    CMVideoFormatDescriptionRef formatDesc;
}

- (id)init
{
    self = [super init];
    
    displayLayer = [[AVSampleBufferDisplayLayer alloc] init];
    displayLayer.bounds = CGRectMake(0, 0, 300, 300);
    displayLayer.backgroundColor = [UIColor blackColor].CGColor;
    displayLayer.position = CGPointMake(500, 500);
    
    // We need some parameter sets before we can properly start decoding frames
    waitingForSps = true;
    waitingForPpsA = true;
    waitingForPpsB = true;
    
    return self;
}

#define ES_START_PREFIX_SIZE 4
#define ES_DATA_OFFSET 5
- (void)submitDecodeBuffer:(unsigned char *)data length:(int)length
{
    unsigned char nalType = data[ES_START_PREFIX_SIZE] & 0x1F;
    OSStatus status;
    
    if (formatDesc == NULL && (nalType == 0x7 || nalType == 0x8)) {
        if (waitingForSps && nalType == 0x7) {
            spsData = [NSData dataWithBytes:&data[ES_DATA_OFFSET] length:length - ES_DATA_OFFSET];
            waitingForSps = false;
        }
        // Nvidia's stream has 2 PPS NALUs so we'll wait for both of them
        else if ((waitingForPpsA || waitingForPpsB) && nalType == 0x8) {
            // Read the NALU's PPS index to figure out which PPS this is
            if (data[ES_DATA_OFFSET] == 0) {
                if (waitingForPpsA) {
                    ppsDataA = [NSData dataWithBytes:&data[ES_DATA_OFFSET] length:length - ES_DATA_OFFSET];
                    waitingForPpsA = false;
                }
            }
            else if (data[ES_DATA_OFFSET] == 1) {
                if (waitingForPpsB) {
                    ppsDataA = [NSData dataWithBytes:&data[ES_DATA_OFFSET] length:length - ES_DATA_OFFSET];
                    waitingForPpsB = false;
                }
            }
        }
        
        // See if we've got all the parameter sets we need
        if (!waitingForSps && !waitingForPpsA && !waitingForPpsB) {
            const uint8_t* const parameterSetPointers[] = { [spsData bytes], [ppsDataA bytes], [ppsDataB bytes] };
            const size_t parameterSetSizes[] = { [spsData length], [ppsDataA length], [ppsDataB length] };
            
            status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                         3, /* count of parameter sets */
                                                                         parameterSetPointers,
                                                                         parameterSetSizes,
                                                                         4 /* size of length prefix */,
                                                                         &formatDesc);
            if (status != noErr) {
                NSLog(@"Failed to create format description: %d", (int)status);
                formatDesc = NULL;
                return;
            }
        }
        
        // No frame data to submit for these NALUs
        return;
    }
    
    if (formatDesc == NULL) {
        // Can't decode if we haven't gotten our parameter sets yet
        return;
    }
    
    // Now we're decoding actual frame data here
    CMBlockBufferRef blockBuffer;
    status = CMBlockBufferCreateWithMemoryBlock(NULL, data, length, kCFAllocatorNull, NULL, 0, length, 0, &blockBuffer);
    if (status != noErr) {
        NSLog(@"CMBlockBufferCreateWithMemoryBlock failed: %d", (int)status);
        return;
    }
    
    // Compute the new length prefix to replace the 00 00 00 01
    const uint8_t lengthBytes[] = {(uint8_t)(length >> 24), (uint8_t)(length >> 16), (uint8_t)(length >> 8), (uint8_t)length};
    status = CMBlockBufferReplaceDataBytes(lengthBytes, blockBuffer, 0, 4);
    if (status != noErr) {
        NSLog(@"CMBlockBufferReplaceDataBytes failed: %d", (int)status);
        return;
    }
    
    CMSampleBufferRef sampleBuffer;
    const size_t sampleSizeArray[] = {length};
    
    status = CMSampleBufferCreate(kCFAllocatorDefault,
                                  blockBuffer, true, NULL,
                                  NULL, formatDesc, 1, 0,
                                  NULL, 1, sampleSizeArray,
                                  &sampleBuffer);
    if (status != noErr) {
        NSLog(@"CMSampleBufferCreate failed: %d", (int)status);
        return;
    }
    
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, YES);
    CFMutableDictionaryRef dict = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
    CFDictionarySetValue(dict, kCMSampleAttachmentKey_DisplayImmediately, kCFBooleanTrue);
    
    dispatch_async(dispatch_get_main_queue(),^{
        [displayLayer enqueueSampleBuffer:sampleBuffer];
        [displayLayer setNeedsDisplay];
    });
}

@end
