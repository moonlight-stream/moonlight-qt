//
//  Connection.m
//  Limelight-iOS
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2014 Diego Waxemberg. All rights reserved.
//

#import "Connection.h"
#import <AudioUnit/AudioUnit.h>
#import <AVFoundation/AVFoundation.h>

#include "Limelight.h"
#include "opus.h"

@implementation Connection {
    IP_ADDRESS host;
    STREAM_CONFIGURATION streamConfig;
    CONNECTION_LISTENER_CALLBACKS clCallbacks;
    DECODER_RENDERER_CALLBACKS drCallbacks;
    AUDIO_RENDERER_CALLBACKS arCallbacks;
}

static OpusDecoder *opusDecoder;


#define PCM_BUFFER_SIZE 1024
#define OUTPUT_BUS 0

static short* decodedPcmBuffer;
static int filledPcmBuffer;
NSLock* audioRendererBlock;
AudioComponentInstance audioUnit;
bool started = false;


void DrSetup(int width, int height, int fps, void* context, int drFlags)
{
    printf("Setup video\n");
}

void DrSubmitDecodeUnit(PDECODE_UNIT decodeUnit)
{
    unsigned char* data = (unsigned char*) malloc(decodeUnit->fullLength);
    if (data != NULL) {
        int offset = 0;
        
        PLENTRY entry = decodeUnit->bufferList;
        while (entry != NULL) {
            memcpy(&data[offset], entry->data, entry->length);
            offset += entry->length;
            entry = entry->next;
        }
        
        // FIXME: Submit data to decoder
        
        free(data);
    }
}

void DrStart(void)
{
    printf("Start video\n");
}

void DrStop(void)
{
    printf("Stop video\n");
}

void DrRelease(void)
{
    printf("Release video\n");
}

void ArInit(void)
{
    int err;
    
    printf("Init audio\n");
    
    opusDecoder = opus_decoder_create(48000, 2, &err);
    
    decodedPcmBuffer = malloc(PCM_BUFFER_SIZE);
}

void ArRelease(void)
{
    printf("Release audio\n");
    
    if (decodedPcmBuffer != NULL) {
        free(decodedPcmBuffer);
        decodedPcmBuffer = NULL;
    }
    
    if (opusDecoder != NULL) {
        opus_decoder_destroy(opusDecoder);
        opusDecoder = NULL;
    }
}

void ArStart(void)
{
    printf("Start audio\n");
}

void ArStop(void)
{
    printf("Stop audio\n");
}

void ArDecodeAndPlaySample(char* sampleData, int sampleLength)
{

    if (!started) {
        AudioOutputUnitStart(audioUnit);
        started = true;
    }
    filledPcmBuffer = opus_decode(opusDecoder, (unsigned char*)sampleData, sampleLength, decodedPcmBuffer, PCM_BUFFER_SIZE / 2, 0);
    if (filledPcmBuffer > 0) {
        // Return of opus_decode is samples per channel
        filledPcmBuffer *= 4;
        
        NSLog(@"pcmBuffer: %d", filledPcmBuffer);        
    }
}

-(id) initWithHost:(int)ipaddr width:(int)width height:(int)height
{
    self = [super init];
    host = ipaddr;
    
    streamConfig.width = width;
    streamConfig.height = height;
    streamConfig.fps = 30;
    
    drCallbacks.setup = DrSetup;
    drCallbacks.start = DrStart;
    drCallbacks.stop = DrStop;
    drCallbacks.release = DrRelease;
    drCallbacks.submitDecodeUnit = DrSubmitDecodeUnit;
    
    arCallbacks.init = ArInit;
    arCallbacks.start = ArStart;
    arCallbacks.stop = ArStop;
    arCallbacks.release = ArRelease;
    arCallbacks.decodeAndPlaySample = ArDecodeAndPlaySample;
    
    
    //////// Don't think any of this is used /////////
    NSError *audioSessionError = nil;
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];
    [audioSession setPreferredSampleRate:48000.0 error:&audioSessionError];
    
    [audioSession setCategory: AVAudioSessionCategoryPlayAndRecord error: &audioSessionError];
    [audioSession setPreferredOutputNumberOfChannels:2 error:&audioSessionError];
    [audioSession setActive: YES error: &audioSessionError];
    //////////////////////////////////////////////////
    
    OSStatus status;
    
    AudioComponentDescription audioDesc;
    audioDesc.componentType = kAudioUnitType_Output;
    audioDesc.componentSubType = kAudioUnitSubType_RemoteIO;
    audioDesc.componentFlags = 0;
    audioDesc.componentFlagsMask = 0;
    audioDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    status = AudioComponentInstanceNew(AudioComponentFindNext(NULL, &audioDesc), &audioUnit);
    
    if (status) {
        NSLog(@"Unable to instantiate new AudioComponent: %d", (int32_t)status);
    }

    
    AudioStreamBasicDescription audioFormat = {0};
    audioFormat.mSampleRate = 48000;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = 2;
    audioFormat.mBytesPerFrame = 960;
    audioFormat.mBytesPerPacket = 960;
    audioFormat.mReserved = 0;
    
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  OUTPUT_BUS,
                                  &audioFormat,
                                  sizeof(audioFormat));
    if (status) {
        NSLog(@"Unable to set audio unit to output: %d", (int32_t)status);
    }
    
    AURenderCallbackStruct callbackStruct = {0};
    callbackStruct.inputProc = playbackCallback;
    callbackStruct.inputProcRefCon = NULL;
    
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  OUTPUT_BUS,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    if (status) {
        NSLog(@"Unable to set audio unit callback: %d", (int32_t)status);
    }
    
    status = AudioUnitInitialize(audioUnit);
    if (status) {
        NSLog(@"Unable to initialize audioUnit: %d", (int32_t)status);
    }
    
    return self;
}

static OSStatus playbackCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    // Notes: ioData contains buffers (may be more than one!)
    // Fill them up as much as you can. Remember to set the size value in each buffer to match how
    // much data is in the buffer.
    
    NSLog(@"Playback callback");
    
    for (int i = 0; i < ioData->mNumberBuffers; i++) {
        ioData->mBuffers[i].mNumberChannels = 2;
        int min = MIN(ioData->mBuffers[i].mDataByteSize, filledPcmBuffer);
        NSLog(@"Min: %d", min);
        memcpy(ioData->mBuffers[i].mData, decodedPcmBuffer, min);
        ioData->mBuffers[i].mDataByteSize = min;
        filledPcmBuffer -= min;
    }
    
    //[audioRendererBlock unlock];
    return noErr;
}


-(void) main
{
    LiStartConnection(host, &streamConfig, &clCallbacks, &drCallbacks, &arCallbacks, NULL, 0);
}



@end
