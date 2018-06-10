//
//  Connection.m
//  Moonlight
//
//  Created by Diego Waxemberg on 1/19/14.
//  Copyright (c) 2015 Moonlight Stream. All rights reserved.
//

#import "Connection.h"

#import <AudioUnit/AudioUnit.h>
#import <AVFoundation/AVFoundation.h>
#import <VideoToolbox/VideoToolbox.h>

#include "Limelight.h"
#include "opus_multistream.h"

@implementation Connection {
    SERVER_INFORMATION _serverInfo;
    STREAM_CONFIGURATION _streamConfig;
    CONNECTION_LISTENER_CALLBACKS _clCallbacks;
    DECODER_RENDERER_CALLBACKS _drCallbacks;
    AUDIO_RENDERER_CALLBACKS _arCallbacks;
    char _hostString[256];
    char _appVersionString[32];
    char _gfeVersionString[32];
}

static NSLock* initLock;
static OpusMSDecoder* opusDecoder;
static id<ConnectionCallbacks> _callbacks;

#define OUTPUT_BUS 0

#define MAX_CHANNEL_COUNT 2
#define FRAME_SIZE 240

#define CIRCULAR_BUFFER_SIZE 32

static int audioBufferWriteIndex;
static int audioBufferReadIndex;
static int activeChannelCount;
static short audioCircularBuffer[CIRCULAR_BUFFER_SIZE][FRAME_SIZE * MAX_CHANNEL_COUNT];

#define AUDIO_QUEUE_BUFFERS 4

static AudioQueueRef audioQueue;
static AudioQueueBufferRef audioBuffers[AUDIO_QUEUE_BUFFERS];
static VideoDecoderRenderer* renderer;

int DrDecoderSetup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags)
{
    [renderer setupWithVideoFormat:videoFormat];
    return 0;
}

int DrSubmitDecodeUnit(PDECODE_UNIT decodeUnit)
{
    int offset = 0;
    int ret;
    unsigned char* data = (unsigned char*) malloc(decodeUnit->fullLength);
    if (data == NULL) {
        // A frame was lost due to OOM condition
        return DR_NEED_IDR;
    }

    PLENTRY entry = decodeUnit->bufferList;
    while (entry != NULL) {
        // Submit parameter set NALUs directly since no copy is required by the decoder
        if (entry->bufferType != BUFFER_TYPE_PICDATA) {
            ret = [renderer submitDecodeBuffer:(unsigned char*)entry->data length:entry->length bufferType:entry->bufferType];
            if (ret != DR_OK) {
                free(data);
                return ret;
            }
        }
        else {
            memcpy(&data[offset], entry->data, entry->length);
            offset += entry->length;
        }

        entry = entry->next;
    }

    // This function will take our picture data buffer
    return [renderer submitDecodeBuffer:data length:offset bufferType:BUFFER_TYPE_PICDATA];
}

int ArInit(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int flags)
{
    int err;
    
    // Clear the circular buffer
    audioBufferWriteIndex = audioBufferReadIndex = 0;

    // We only support stereo for now.
    // TODO: Ensure AudioToolbox's channel mapping matches Opus's
    // and correct if neccessary.
    assert(audioConfiguration == AUDIO_CONFIGURATION_STEREO);

    activeChannelCount = opusConfig->channelCount;
    opusDecoder = opus_multistream_decoder_create(opusConfig->sampleRate,
                                                  opusConfig->channelCount,
                                                  opusConfig->streams,
                                                  opusConfig->coupledStreams,
                                                  opusConfig->mapping,
                                                  &err);

#if TARGET_OS_IPHONE
    // Configure the audio session for our app
    NSError *audioSessionError = nil;
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];

    [audioSession setPreferredSampleRate:opusConfig->sampleRate error:&audioSessionError];
    [audioSession setCategory: AVAudioSessionCategoryPlayback error: &audioSessionError];
    [audioSession setPreferredOutputNumberOfChannels:opusConfig->channelCount error:&audioSessionError];
    [audioSession setPreferredIOBufferDuration:0.005 error:&audioSessionError];
    [audioSession setActive: YES error: &audioSessionError];
#endif
    
    OSStatus status;
    
    AudioStreamBasicDescription audioFormat = {0};
    audioFormat.mSampleRate = opusConfig->sampleRate;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    audioFormat.mChannelsPerFrame = opusConfig->channelCount;
    audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * (audioFormat.mBitsPerChannel / 8);
    audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame;
    audioFormat.mFramesPerPacket = audioFormat.mBytesPerPacket / audioFormat.mBytesPerFrame;
    audioFormat.mReserved = 0;

    status = AudioQueueNewOutput(&audioFormat, FillOutputBuffer, nil, nil, nil, 0, &audioQueue);
    if (status != noErr) {
        Log(LOG_E, @"Error allocating output queue: %d\n", status);
        return status;
    }
    
    for (int i = 0; i < AUDIO_QUEUE_BUFFERS; i++) {
        status = AudioQueueAllocateBuffer(audioQueue, audioFormat.mBytesPerFrame * FRAME_SIZE, &audioBuffers[i]);
        if (status != noErr) {
            Log(LOG_E, @"Error allocating output buffer: %d\n", status);
            return status;
        }
        
        FillOutputBuffer(nil, audioQueue, audioBuffers[i]);
    }
    
    status = AudioQueueStart(audioQueue, nil);
    if (status != noErr) {
        Log(LOG_E, @"Error starting queue: %d\n", status);
        return status;
    }
    
    return status;
}

void ArCleanup(void)
{
    if (opusDecoder != NULL) {
        opus_multistream_decoder_destroy(opusDecoder);
        opusDecoder = NULL;
    }
    
    // Stop before disposing to avoid massive delay inside
    // AudioQueueDispose() (iOS bug?)
    AudioQueueStop(audioQueue, true);
    
    // Also frees buffers
    AudioQueueDispose(audioQueue, true);
    
#if TARGET_OS_IPHONE
    // Audio session is now inactive
    AVAudioSession* audioSession = [AVAudioSession sharedInstance];
    [audioSession setActive: YES error: nil];
#endif
}

void ArDecodeAndPlaySample(char* sampleData, int sampleLength)
{
    int decodeLen;
    
    // Check if there is space for this sample in the buffer. Again, this can race
    // but in the worst case, we'll not see the sample callback having consumed a sample.
    if (((audioBufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE) == audioBufferReadIndex) {
        return;
    }
    
    decodeLen = opus_multistream_decode(opusDecoder, (unsigned char *)sampleData, sampleLength,
                                        audioCircularBuffer[audioBufferWriteIndex], FRAME_SIZE, 0);
    if (decodeLen > 0) {
        // Use a full memory barrier to ensure the circular buffer is written before incrementing the index
        __sync_synchronize();
        
        // This can race with the reader in the sample callback, however this is a benign
        // race since we'll either read the original value of s_WriteIndex (which is safe,
        // we just won't consider this sample) or the new value of s_WriteIndex
        audioBufferWriteIndex = (audioBufferWriteIndex + 1) % CIRCULAR_BUFFER_SIZE;
    }
}

void ClStageStarting(int stage)
{
    [_callbacks stageStarting:LiGetStageName(stage)];
}

void ClStageComplete(int stage)
{
    [_callbacks stageComplete:LiGetStageName(stage)];
}

void ClStageFailed(int stage, long errorCode)
{
    [_callbacks stageFailed:LiGetStageName(stage) withError:errorCode];
}

void ClConnectionStarted(void)
{
    [_callbacks connectionStarted];
}

void ClConnectionTerminated(long errorCode)
{
    [_callbacks connectionTerminated: errorCode];
}

void ClDisplayMessage(const char* message)
{
    [_callbacks displayMessage: message];
}

void ClDisplayTransientMessage(const char* message)
{
    [_callbacks displayTransientMessage: message];
}

void ClLogMessage(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
}

-(void) terminate
{
    // Interrupt any action blocking LiStartConnection(). This is
    // thread-safe and done outside initLock on purpose, since we
    // won't be able to acquire it if LiStartConnection is in
    // progress.
    LiInterruptConnection();
    
    // We dispatch this async to get out because this can be invoked
    // on a thread inside common and we don't want to deadlock. It also avoids
    // blocking on the caller's thread waiting to acquire initLock.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        [initLock lock];
        LiStopConnection();
        [initLock unlock];
    });
}

-(id) initWithConfig:(StreamConfiguration*)config renderer:(VideoDecoderRenderer*)myRenderer connectionCallbacks:(id<ConnectionCallbacks>)callbacks
{
    self = [super init];

    // Use a lock to ensure that only one thread is initializing
    // or deinitializing a connection at a time.
    if (initLock == nil) {
        initLock = [[NSLock alloc] init];
    }
    
    strncpy(_hostString,
            [config.host cStringUsingEncoding:NSUTF8StringEncoding],
            sizeof(_hostString));
    strncpy(_appVersionString,
            [config.appVersion cStringUsingEncoding:NSUTF8StringEncoding],
            sizeof(_appVersionString));
    if (config.gfeVersion != nil) {
        strncpy(_gfeVersionString,
                [config.gfeVersion cStringUsingEncoding:NSUTF8StringEncoding],
                sizeof(_gfeVersionString));
    }

    LiInitializeServerInformation(&_serverInfo);
    _serverInfo.address = _hostString;
    _serverInfo.serverInfoAppVersion = _appVersionString;
    if (config.gfeVersion != nil) {
        _serverInfo.serverInfoGfeVersion = _gfeVersionString;
    }

    renderer = myRenderer;
    _callbacks = callbacks;

    LiInitializeStreamConfiguration(&_streamConfig);
    _streamConfig.width = config.width;
    _streamConfig.height = config.height;
    _streamConfig.fps = config.frameRate;
    _streamConfig.bitrate = config.bitRate;
    _streamConfig.streamingRemotely = config.streamingRemotely;
    _streamConfig.enableHdr = config.enableHdr;
    
    switch (config.audioChannelCount) {
        case 2:
            _streamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            break;
        case 6:
            _streamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
            break;
        default:
            Log(LOG_E, @"Unknown audio channel count: %d", config.audioChannelCount);
            abort();
    }
    
    // HDR implies HEVC allowed
    if (config.enableHdr) {
        config.allowHevc = YES;
    }

#if TARGET_OS_IPHONE
    // On iOS 11, we can use HEVC if the server supports encoding it
    // and this device has hardware decode for it (A9 and later).
    // Additionally, iPhone X had a bug which would cause video
    // to freeze after a few minutes with HEVC prior to iOS 11.3.
    // As a result, we will only use HEVC on iOS 11.3 or later.
    if (@available(iOS 11.3, *)) {
        _streamConfig.supportsHevc = config.allowHevc && VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC);
    }
#else
    if (@available(macOS 10.13, *)) {
        // Streaming with limited bandwidth will result in better quality with HEVC
        if (VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC) || _streamConfig.streamingRemotely != 0)
            _streamConfig.supportsHevc = config.allowHevc;
    }
#endif
    
    // HEVC must be supported when HDR is enabled
    assert(!_streamConfig.enableHdr || _streamConfig.supportsHevc);
    
    // Use some of the HEVC encoding efficiency improvements to
    // reduce bandwidth usage while still gaining some image
    // quality improvement.
    if (config.streamingRemotely) {
        // In the case of remotely streaming, we want the best possible qualtity for a limited bandwidth, so we set the multiplier to 0
        _streamConfig.hevcBitratePercentageMultiplier = 0;
        // When streaming remotely we want to use a packet size of 1024
        _streamConfig.packetSize = 1024;
    }
    else {
        _streamConfig.hevcBitratePercentageMultiplier = 75;
        _streamConfig.packetSize = 1292;
    }

    memcpy(_streamConfig.remoteInputAesKey, [config.riKey bytes], [config.riKey length]);
    memset(_streamConfig.remoteInputAesIv, 0, 16);
    int riKeyId = htonl(config.riKeyId);
    memcpy(_streamConfig.remoteInputAesIv, &riKeyId, sizeof(riKeyId));

    LiInitializeVideoCallbacks(&_drCallbacks);
    _drCallbacks.setup = DrDecoderSetup;
    _drCallbacks.submitDecodeUnit = DrSubmitDecodeUnit;

    // RFI doesn't work properly with HEVC on iOS 11 with an iPhone SE (at least)
    // It doesnt work on macOS either, tested with Network Link Conditioner.
    _drCallbacks.capabilities = CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC |
                                CAPABILITY_DIRECT_SUBMIT;

    LiInitializeAudioCallbacks(&_arCallbacks);
    _arCallbacks.init = ArInit;
    _arCallbacks.cleanup = ArCleanup;
    _arCallbacks.decodeAndPlaySample = ArDecodeAndPlaySample;
    _arCallbacks.capabilities = CAPABILITY_DIRECT_SUBMIT;

    LiInitializeConnectionCallbacks(&_clCallbacks);
    _clCallbacks.stageStarting = ClStageStarting;
    _clCallbacks.stageComplete = ClStageComplete;
    _clCallbacks.stageFailed = ClStageFailed;
    _clCallbacks.connectionStarted = ClConnectionStarted;
    _clCallbacks.connectionTerminated = ClConnectionTerminated;
    _clCallbacks.displayMessage = ClDisplayMessage;
    _clCallbacks.displayTransientMessage = ClDisplayTransientMessage;
    _clCallbacks.logMessage = ClLogMessage;

    return self;
}

static void FillOutputBuffer(void *aqData,
                             AudioQueueRef inAQ,
                             AudioQueueBufferRef inBuffer) {
    inBuffer->mAudioDataByteSize = activeChannelCount * FRAME_SIZE * sizeof(short);
    
    assert(inBuffer->mAudioDataByteSize == inBuffer->mAudioDataBytesCapacity);
    
    // If the indexes aren't equal, we have a sample
    if (audioBufferWriteIndex != audioBufferReadIndex) {
        // Copy data to the audio buffer
        memcpy(inBuffer->mAudioData,
               audioCircularBuffer[audioBufferReadIndex],
               inBuffer->mAudioDataByteSize);
        
        // Use a full memory barrier to ensure the circular buffer is read before incrementing the index
        __sync_synchronize();
        
        // This can race with the reader in the AudDecDecodeAndPlaySample function. This is
        // not a problem because at worst, it just won't see that we've consumed this sample yet.
        audioBufferReadIndex = (audioBufferReadIndex + 1) % CIRCULAR_BUFFER_SIZE;
    }
    else {
        // No data, so play silence
        memset(inBuffer->mAudioData, 0, inBuffer->mAudioDataByteSize);
    }
    
    AudioQueueEnqueueBuffer(inAQ, inBuffer, 0, NULL);
}

-(void) main
{
    [initLock lock];
    LiStartConnection(&_serverInfo,
                      &_streamConfig,
                      &_clCallbacks,
                      &_drCallbacks,
                      &_arCallbacks,
                      NULL, 0,
                      NULL, 0);
    [initLock unlock];
}

@end
