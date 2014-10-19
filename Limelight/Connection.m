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

struct AUDIO_BUFFER_QUEUE_ENTRY {
    struct AUDIO_BUFFER_QUEUE_ENTRY *next;
    int length;
    int offset;
    char data[0];
};

static short decodedPcmBuffer[512];
static NSLock *audioLock;
static struct AUDIO_BUFFER_QUEUE_ENTRY *audioBufferQueue;
static AudioComponentInstance audioUnit;
static VideoDecoderRenderer* renderer;

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
        
        [renderer submitDecodeBuffer:data length:decodeUnit->fullLength];
        
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
    
    audioLock = [[NSLock alloc] init];
}

void ArRelease(void)
{
    printf("Release audio\n");
    
    if (opusDecoder != NULL) {
        opus_decoder_destroy(opusDecoder);
        opusDecoder = NULL;
    }
}

void ArStart(void)
{
    printf("Start audio\n");
    AudioOutputUnitStart(audioUnit);
}

void ArStop(void)
{
    printf("Stop audio\n");
}

void ArDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int decodedLength = opus_decode(opusDecoder, (unsigned char*)sampleData, sampleLength, decodedPcmBuffer, PCM_BUFFER_SIZE / 2, 0);
    if (decodedLength > 0) {
        // Return of opus_decode is samples per channel
        decodedLength *= 4;
        
        struct AUDIO_BUFFER_QUEUE_ENTRY *newEntry = malloc(sizeof(*newEntry) + decodedLength);
        if (newEntry != NULL) {
            newEntry->next = NULL;
            newEntry->length = decodedLength;
            newEntry->offset = 0;
            memcpy(newEntry->data, decodedPcmBuffer, decodedLength);
            
            [audioLock lock];
            if (audioBufferQueue == NULL) {
                audioBufferQueue = newEntry;
            }
            else {
                struct AUDIO_BUFFER_QUEUE_ENTRY *lastEntry = audioBufferQueue;
                while (lastEntry->next != NULL) {
                    lastEntry = lastEntry->next;
                }
                lastEntry->next = newEntry;
            }
            [audioLock unlock];
        }
    }
}

void ClStageStarting(int stage)
{
    NSLog(@"Starting stage: %d", stage);
}

void ClStageComplete(int stage)
{
    NSLog(@"Stage %d complete", stage);
}

void ClStageFailed(int stage, long errorCode)
{
    NSLog(@"Stage %d failed: %ld", stage, errorCode);
}

void ClConnectionStarted(void)
{
    NSLog(@"Connection started");
}

void ClConnectionTerminated(long errorCode)
{
    NSLog(@"ConnectionTerminated: %ld", errorCode);
}

void ClDisplayMessage(char* message)
{
    NSLog(@"DisplayMessage: %s", message);
}

void ClDisplayTransientMessage(char* message)
{
    NSLog(@"DisplayTransientMessage: %s", message);
}

-(id) initWithHost:(int)ipaddr width:(int)width height:(int)height renderer:(VideoDecoderRenderer*)myRenderer
{
    self = [super init];
    host = ipaddr;
    renderer = myRenderer;
    
    streamConfig.width = width;
    streamConfig.height = height;
    streamConfig.fps = 30;
    streamConfig.bitrate = 5000;
    streamConfig.packetSize = 1024;
    // FIXME: RI AES members
    
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
    
    clCallbacks.stageStarting = ClStageStarting;
    clCallbacks.stageComplete = ClStageComplete;
    clCallbacks.stageFailed = ClStageFailed;
    clCallbacks.connectionStarted = ClConnectionStarted;
    clCallbacks.connectionTerminated = ClConnectionTerminated;
    clCallbacks.displayMessage = ClDisplayMessage;
    clCallbacks.displayTransientMessage = ClDisplayTransientMessage;
    
    // Configure the audio session for our app
    NSError *audioSessionError = nil;
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];
    
    [audioSession setPreferredSampleRate:48000.0 error:&audioSessionError];
    [audioSession setCategory: AVAudioSessionCategoryPlayAndRecord error: &audioSessionError];
    [audioSession setPreferredOutputNumberOfChannels:2 error:&audioSessionError];
    [audioSession setPreferredIOBufferDuration:0.005 error:&audioSessionError];
    [audioSession setActive: YES error: &audioSessionError];
    
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
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    audioFormat.mChannelsPerFrame = 2;
    audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * (audioFormat.mBitsPerChannel / 8);
    audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame;
    audioFormat.mFramesPerPacket = audioFormat.mBytesPerPacket / audioFormat.mBytesPerFrame;
    audioFormat.mReserved = 0;

    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  OUTPUT_BUS,
                                  &audioFormat,
                                  sizeof(audioFormat));
    if (status) {
        NSLog(@"Unable to set audio unit to input: %d", (int32_t)status);
    }
    
    AURenderCallbackStruct callbackStruct = {0};
    callbackStruct.inputProc = playbackCallback;
    callbackStruct.inputProcRefCon = NULL;
    
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input,
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
    
    for (int i = 0; i < ioData->mNumberBuffers; i++) {
        ioData->mBuffers[i].mNumberChannels = 2;
        
        if (ioData->mBuffers[i].mDataByteSize != 0) {
            int thisBufferOffset = 0;
            
        FillBufferAgain:
            // Make sure there's data to write
            if (ioData->mBuffers[i].mDataByteSize - thisBufferOffset == 0) {
                continue;
            }
            
            // Wait for a buffer to be available
            // FIXME: This needs optimization to avoid busy waiting for buffers
            struct AUDIO_BUFFER_QUEUE_ENTRY *audioEntry = NULL;
            while (audioEntry == NULL)
            {
                [audioLock lock];
                if (audioBufferQueue != NULL) {
                    // Dequeue this entry temporarily
                    audioEntry = audioBufferQueue;
                    audioBufferQueue = audioBufferQueue->next;
                }
                [audioLock unlock];
            }
            
            // Figure out how much data we can write
            int min = MIN(ioData->mBuffers[i].mDataByteSize - thisBufferOffset, audioEntry->length);
            
            // Copy data to the audio buffer
            memcpy(&ioData->mBuffers[i].mData[thisBufferOffset], &audioEntry->data[audioEntry->offset], min);
            thisBufferOffset += min;
            
            if (min < audioEntry->length) {
                // This entry still has unused data
                audioEntry->length -= min;
                audioEntry->offset += min;
                
                // Requeue the entry
                [audioLock lock];
                audioEntry->next = audioBufferQueue;
                audioBufferQueue = audioEntry;
                [audioLock unlock];
            }
            else {
                // This entry is fully depleted so free it
                free(audioEntry);
                
                // Try to grab another sample to fill this buffer with
                goto FillBufferAgain;
            }

            ioData->mBuffers[i].mDataByteSize = thisBufferOffset;
        }
    }
    
    return noErr;
}


-(void) main
{
    LiStartConnection(host, &streamConfig, &clCallbacks, &drCallbacks, &arCallbacks, NULL, 0);
}



@end
