#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <opus.h>
#import <CommonCrypto/CommonCryptor.h>
#import <arpa/inet.h>
#import <unistd.h>
#import <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <fcntl.h>
#include "native-mic.h"

static atomic_bool capturing;
static atomic_bool captureFailed;

typedef struct {
    OpusEncoder *encoder;
    int fd;
    uint16_t seq;
    uint32_t timestamp, ssrc;
    uint64_t nextPingNs;
    bool encryptAudio;
    uint8_t key[kCCKeySizeAES128];
    uint32_t keyId;
    struct sockaddr_in dest;
    int16_t samples[240];
    int count;
} Sender;

static int setup(Sender *s, const char *ip, int port, const char *key, const char *sessionIv) {
    s->fd = -1;
    if (!key || !sessionIv) return -1; // No implicit plaintext microphone fallback.
    s->dest.sin_family = AF_INET;
    s->dest.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &s->dest.sin_addr) != 1 || port < 1 || port > 65535) return -1;
    uint32_t a = ntohl(s->dest.sin_addr.s_addr);
    if (a >> 24 != 10 && a >> 24 != 127 && a >> 20 != 0xac1 && a >> 16 != 0xc0a8 && a >> 22 != 0x191) return -1;
    int err;
    s->encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
    if (err || opus_encoder_ctl(s->encoder, OPUS_SET_BITRATE(32000)) ||
        opus_encoder_ctl(s->encoder, OPUS_SET_VBR(0)) || opus_encoder_ctl(s->encoder, OPUS_SET_DTX(0))) return -1;
    if (key) {
        s->encryptAudio = true;
        memcpy(s->key, key, sizeof(s->key));
        uint32_t networkId;
        memcpy(&networkId, sessionIv, sizeof(networkId));
        s->keyId = ntohl(networkId);
    }
    s->seq = 0; s->timestamp = 0; s->ssrc = 1;
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->fd < 0) return -1;
    if (fcntl(s->fd, F_SETFL, O_NONBLOCK) < 0) return -1;
    struct sockaddr_in local = {.sin_family = AF_INET, .sin_port = htons(50002)};
    return bind(s->fd, (struct sockaddr *)&local, sizeof(local));
}
static void cleanup(Sender *s) {
    if (s->fd >= 0) close(s->fd);
    if (s->encoder) opus_encoder_destroy(s->encoder);
    memset_s(s->key, sizeof(s->key), 0, sizeof(s->key));
    *s = (Sender){.fd=-1};
}
static int packet(Sender *s, const int16_t *pcm) {
    uint64_t now = clock_gettime_nsec_np(CLOCK_MONOTONIC);
    if (now >= s->nextPingNs) {
        if (sendto(s->fd, "PING", 4, 0, (struct sockaddr *)&s->dest, sizeof(s->dest)) != 4) return -1;
        s->nextPingNs = now + 500000000ULL;
    }
    uint8_t p[12 + 32] = {0x80,97};
    uint16_t seq = htons(s->seq++);
    uint32_t stamp = htonl(s->timestamp), ssrc = htonl(s->ssrc);
    s->timestamp += 5; // NVIDIA microphone timestamps count milliseconds.
    memcpy(p+2,&seq,2); memcpy(p+4,&stamp,4); memcpy(p+8,&ssrc,4);
    uint8_t opus[20];
    if (opus_encode(s->encoder, pcm, 240, opus, sizeof(opus)) != sizeof(opus)) return -1;
    size_t payloadSize = sizeof(opus);
    if (s->encryptAudio) {
        uint8_t iv[kCCBlockSizeAES128] = {0};
        uint32_t prefix = htonl(s->keyId + ntohs(seq));
        memcpy(iv, &prefix, sizeof(prefix));
        if (CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding,
                    s->key, sizeof(s->key), iv, opus, sizeof(opus),
                    p + 12, sizeof(p) - 12, &payloadSize) != kCCSuccess) return -1;
    } else {
        memcpy(p + 12, opus, payloadSize);
    }
    size_t size = 12 + payloadSize;
    return sendto(s->fd, p, size, 0, (struct sockaddr *)&s->dest, sizeof(s->dest)) == (ssize_t)size ? 0 : -1;
}
static void captured(void *ctx, AudioQueueRef q, AudioQueueBufferRef b,
                     const AudioTimeStamp *time, UInt32 packets, const AudioStreamPacketDescription *desc) {
    Sender *s = ctx;
    if (!atomic_load(&capturing)) return;
    if (b->mAudioDataByteSize > b->mAudioDataBytesCapacity || b->mAudioDataByteSize % 2) {
        atomic_store(&captureFailed, true);
        atomic_store(&capturing, false);
        return;
    }
    const int16_t *pcm = b->mAudioData;
    for (UInt32 i=0; i < b->mAudioDataByteSize/2; i++) {
        s->samples[s->count++] = pcm[i];
        if (s->count == 240) {
            if (packet(s, s->samples)) {
                atomic_store(&captureFailed, true);
                atomic_store(&capturing, false);
                return;
            }
            s->count = 0;
        }
    }
    if (atomic_load(&capturing) && AudioQueueEnqueueBuffer(q,b,0,NULL)) {
        atomic_store(&captureFailed, true);
        atomic_store(&capturing, false);
    }
}

static void queueStateChanged(void *unused, AudioQueueRef q, AudioQueuePropertyID property) {
    UInt32 running = 0;
    UInt32 size = sizeof(running);
    if (property == kAudioQueueProperty_IsRunning &&
            AudioQueueGetProperty(q, property, &running, &size) == noErr &&
            !running && atomic_load(&capturing)) {
        atomic_store(&captureFailed, true);
        atomic_store(&capturing, false);
    }
}

static Sender sender={.fd=-1};
static AudioQueueRef queue;
static pthread_t toneThread;
static bool toneRunning;
static atomic_bool cancelTone;
static void *runTone(void *unused) {
    // Let the Windows capture endpoint open before sending the diagnostic tone.
    for (int i = 0; i < 60 && !atomic_load(&cancelTone); ++i) usleep(50000);
    uint64_t start = clock_gettime_nsec_np(CLOCK_MONOTONIC);
    for (int frame=0; frame<4000 && !atomic_load(&cancelTone); frame++) {
        int16_t pcm[240];
        for(int i=0;i<240;i++) pcm[i]=(int16_t)(8000*sin(2*M_PI*(i%48)/48.0));
        if(packet(&sender,pcm)) {atomic_store(&captureFailed,true);break;}
        int64_t remaining=(int64_t)(start+(uint64_t)(frame+1)*5000000)-(int64_t)clock_gettime_nsec_np(CLOCK_MONOTONIC);
        if(remaining>0) usleep((useconds_t)(remaining/1000));
    }
    atomic_store(&capturing,false);
    return NULL;
}
int nativeMicStatus(void) {
    return atomic_load(&captureFailed) ? -1 : (atomic_load(&capturing) ? 1 : 0);
}
void stopNativeMic(void) {
    atomic_store(&capturing, false);
    if(toneRunning){atomic_store(&cancelTone,true);pthread_join(toneThread,NULL);toneRunning=false;}
    if(queue){ AudioQueueStop(queue,true); AudioQueueDispose(queue,true); queue=NULL; }
    cleanup(&sender);
    atomic_store(&captureFailed,false);
}
int startNativeTone(const char *host, unsigned short port, const char *key, const char *sessionIv) {
    stopNativeMic();
    atomic_store(&captureFailed,false);
    if(setup(&sender,host,port,key,sessionIv)){cleanup(&sender);return 3;}
    atomic_store(&cancelTone,false);
    if(pthread_create(&toneThread,NULL,runTone,NULL)){cleanup(&sender);return 4;}
    toneRunning=true;
    atomic_store(&capturing,true);
    return 0;
}
int startNativeMic(const char *host, unsigned short port, const char *key, const char *sessionIv) {
    stopNativeMic();
    atomic_store(&captureFailed,false);
    if (@available(macOS 10.14, *)) {
    AVAuthorizationStatus auth=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if(auth==AVAuthorizationStatusNotDetermined){
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted){}];
        return 1; // Reconnect after granting access; never start capture from a stale callback.
    }
    if(auth!=AVAuthorizationStatusAuthorized)return 2;
    } else { return 5; }
    if(setup(&sender,host,port,key,sessionIv)){cleanup(&sender);return 3;}
    AudioStreamBasicDescription f={.mSampleRate=48000,.mFormatID=kAudioFormatLinearPCM,
                .mFormatFlags=kLinearPCMFormatFlagIsSignedInteger|kLinearPCMFormatFlagIsPacked,
                .mBytesPerPacket=2,.mFramesPerPacket=1,.mBytesPerFrame=2,.mChannelsPerFrame=1,.mBitsPerChannel=16};
            OSStatus e=AudioQueueNewInput(&f,captured,&sender,NULL,NULL,0,&queue);
            for(int i=0;!e && i<4;i++){AudioQueueBufferRef b; e=AudioQueueAllocateBuffer(queue,480,&b); if(!e)e=AudioQueueEnqueueBuffer(queue,b,0,NULL);}
            if(!e)e=AudioQueueAddPropertyListener(queue,kAudioQueueProperty_IsRunning,queueStateChanged,NULL);
            if(!e) {
                atomic_store(&capturing,true);
                e=AudioQueueStart(queue,NULL);
            }
    if(e)stopNativeMic();
    return (int)e;
}
