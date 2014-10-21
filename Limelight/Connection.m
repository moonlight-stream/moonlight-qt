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
    IP_ADDRESS _host;
    STREAM_CONFIGURATION _streamConfig;
    CONNECTION_LISTENER_CALLBACKS _clCallbacks;
    DECODER_RENDERER_CALLBACKS _drCallbacks;
    AUDIO_RENDERER_CALLBACKS _arCallbacks;
    
}

static OpusDecoder *opusDecoder;
static id<ConTermCallback> _callback;

#define PCM_BUFFER_SIZE 1024
#define OUTPUT_BUS 0

struct AUDIO_BUFFER_QUEUE_ENTRY {
    struct AUDIO_BUFFER_QUEUE_ENTRY *next;
    int length;
    int offset;
    char data[0];
};

#define MAX_QUEUE_ENTRIES 10

static short decodedPcmBuffer[512];
static NSLock *audioLock;
static struct AUDIO_BUFFER_QUEUE_ENTRY *audioBufferQueue;
static int audioBufferQueueLength;
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
        
        // This function will take our buffer
        [renderer submitDecodeBuffer:data length:decodeUnit->fullLength];
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
            if (audioBufferQueueLength > MAX_QUEUE_ENTRIES) {
                NSLog(@"Audio player too slow. Dropping all decoded samples!");
                
                // Clear all values from the buffer queue
                struct AUDIO_BUFFER_QUEUE_ENTRY *entry;
                while (audioBufferQueue != NULL) {
                    entry = audioBufferQueue;
                    audioBufferQueue = entry->next;
                    audioBufferQueueLength--;
                    free(entry);
                }
            }
            
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
            audioBufferQueueLength++;
            
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
    
    [_callback connectionTerminated];
}

void ClDisplayMessage(char* message)
{
    NSLog(@"DisplayMessage: %s", message);
}

void ClDisplayTransientMessage(char* message)
{
    NSLog(@"DisplayTransientMessage: %s", message);
}

-(void) terminate
{
    // We dispatch this async to get out because this can be invoked
    // on a thread inside common and we don't want to deadlock
    dispatch_async(dispatch_get_main_queue(), ^{
        // This is safe to call even before LiStartConnection
        LiStopConnection();
    });
}

-(id) initWithConfig:(StreamConfiguration*)config renderer:(VideoDecoderRenderer*)myRenderer connectionTerminatedCallback:(id<ConTermCallback>)callback
{
    self = [super init];
    _host = config.hostAddr;
    renderer = myRenderer;
    _callback = callback;
    _streamConfig.width = config.width;
    _streamConfig.height = config.height;
    _streamConfig.fps = config.frameRate;
    _streamConfig.bitrate = config.bitRate;
    _streamConfig.packetSize = 1024;
    
    memcpy(_streamConfig.remoteInputAesKey, [config.riKey bytes], [config.riKey length]);
    memset(_streamConfig.remoteInputAesIv, 0, 16);
    int riKeyId = htonl(config.riKeyId);
    memcpy(_streamConfig.remoteInputAesIv, &riKeyId, sizeof(riKeyId));
    
    _drCallbacks.setup = DrSetup;
    _drCallbacks.start = DrStart;
    _drCallbacks.stop = DrStop;
    _drCallbacks.release = DrRelease;
    _drCallbacks.submitDecodeUnit = DrSubmitDecodeUnit;
    
    _arCallbacks.init = ArInit;
    _arCallbacks.start = ArStart;
    _arCallbacks.stop = ArStop;
    _arCallbacks.release = ArRelease;
    _arCallbacks.decodeAndPlaySample = ArDecodeAndPlaySample;
    
    _clCallbacks.stageStarting = ClStageStarting;
    _clCallbacks.stageComplete = ClStageComplete;
    _clCallbacks.stageFailed = ClStageFailed;
    _clCallbacks.connectionStarted = ClConnectionStarted;
    _clCallbacks.connectionTerminated = ClConnectionTerminated;
    _clCallbacks.displayMessage = ClDisplayMessage;
    _clCallbacks.displayTransientMessage = ClDisplayTransientMessage;
    
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
    
    // We start here because it seems to need some warmup time
    // before it starts accepting samples
    status = AudioOutputUnitStart(audioUnit);
    if (status) {
        NSLog(@"Unable to start audioUnit: %d", (int32_t)status);
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
    
    bool ranOutOfData = false;
    for (int i = 0; i < ioData->mNumberBuffers; i++) {
        ioData->mBuffers[i].mNumberChannels = 2;
        
        if (ranOutOfData) {
            ioData->mBuffers[i].mDataByteSize = 0;
            continue;
        }
        
        if (ioData->mBuffers[i].mDataByteSize != 0) {
            int thisBufferOffset = 0;
            
        FillBufferAgain:
            // Make sure there's data to write
            if (ioData->mBuffers[i].mDataByteSize - thisBufferOffset == 0) {
                continue;
            }
            
            struct AUDIO_BUFFER_QUEUE_ENTRY *audioEntry = NULL;
            
            [audioLock lock];
            if (audioBufferQueue != NULL) {
                // Dequeue this entry temporarily
                audioEntry = audioBufferQueue;
                audioBufferQueue = audioBufferQueue->next;
                audioBufferQueueLength--;
            }
            [audioLock unlock];
            
            if (audioEntry == NULL) {
                // No data left
                ranOutOfData = true;
                ioData->mBuffers[i].mDataByteSize = thisBufferOffset;
                continue;
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
                audioBufferQueueLength++;
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
    LiStartConnection(_host, &_streamConfig, &_clCallbacks, &_drCallbacks, &_arCallbacks, NULL, 0);
}



@end
