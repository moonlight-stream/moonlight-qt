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
    unsigned char ppsDataAFirstByte;
    CMVideoFormatDescriptionRef formatDesc;
}

- (id)initWithView:(UIView*)view
{
    self = [super init];
    
    displayLayer = [[AVSampleBufferDisplayLayer alloc] init];
    displayLayer.bounds = view.bounds;
    displayLayer.backgroundColor = [UIColor greenColor].CGColor;
    displayLayer.position = CGPointMake(CGRectGetMidX(view.bounds), CGRectGetMidY(view.bounds));
    displayLayer.videoGravity = AVLayerVideoGravityResize;
    [view.layer addSublayer:displayLayer];
    
    // We need some parameter sets before we can properly start decoding frames
    waitingForSps = true;
    waitingForPpsA = true;
    waitingForPpsB = true;
    
    return self;
}

#define ES_START_PREFIX_SIZE 4
- (void)submitDecodeBuffer:(unsigned char *)data length:(int)length
{
    unsigned char nalType = data[ES_START_PREFIX_SIZE] & 0x1F;
    OSStatus status;
    
    if (formatDesc == NULL && (nalType == 0x7 || nalType == 0x8)) {
        if (waitingForSps && nalType == 0x7) {
            NSLog(@"Got SPS");
            spsData = [NSData dataWithBytes:&data[ES_START_PREFIX_SIZE] length:length - ES_START_PREFIX_SIZE];
            waitingForSps = false;
        }
        // Nvidia's stream has 2 PPS NALUs so we'll wait for both of them
        else if ((waitingForPpsA || waitingForPpsB) && nalType == 0x8) {
            // Read the NALU's PPS index to figure out which PPS this is
            printf("PPS BYTE: %02x", data[ES_START_PREFIX_SIZE + 1]);
            if (waitingForPpsA) {
                NSLog(@"Got PPS 1");
                ppsDataA = [NSData dataWithBytes:&data[ES_START_PREFIX_SIZE] length:length - ES_START_PREFIX_SIZE];
                waitingForPpsA = false;
                ppsDataAFirstByte = data[ES_START_PREFIX_SIZE + 1];
            }
            else if (data[ES_START_PREFIX_SIZE + 1] != ppsDataAFirstByte) {
                NSLog(@"Got PPS 2");
                ppsDataA = [NSData dataWithBytes:&data[ES_START_PREFIX_SIZE] length:length - ES_START_PREFIX_SIZE];
                waitingForPpsB = false;
            }
        }
        
        // See if we've got all the parameter sets we need
        if (!waitingForSps && !waitingForPpsA && !waitingForPpsB) {
            const uint8_t* const parameterSetPointers[] = { [spsData bytes], [ppsDataA bytes], [ppsDataB bytes] };
            const size_t parameterSetSizes[] = { [spsData length], [ppsDataA length], [ppsDataB length] };
            
            NSLog(@"Constructing format description");
            status = CMVideoFormatDescriptionCreateFromH264ParameterSets(kCFAllocatorDefault,
                                                                         2, /* count of parameter sets */
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
    
    if (nalType != 0x1 && nalType != 0x5) {
        // Don't submit parameter set data
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
    int dataLength = length - ES_START_PREFIX_SIZE;
    const uint8_t lengthBytes[] = {(uint8_t)(dataLength >> 24), (uint8_t)(dataLength >> 16),
        (uint8_t)(dataLength >> 8), (uint8_t)dataLength};
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
    CFDictionarySetValue(dict, kCMSampleAttachmentKey_IsDependedOnByOthers, kCFBooleanTrue);
    
    if (nalType == 1) {
        // P-frame
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanTrue);
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanTrue);
    }
    else {
        // I-frame
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_NotSync, kCFBooleanFalse);
        CFDictionarySetValue(dict, kCMSampleAttachmentKey_DependsOnOthers, kCFBooleanFalse);
    }
    
    [displayLayer enqueueSampleBuffer:sampleBuffer];
}

@end
