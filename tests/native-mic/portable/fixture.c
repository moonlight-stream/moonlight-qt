#include "Limelight-internal.h"
#include <fcntl.h>

struct sockaddr_storage RemoteAddr;
STREAM_CONFIGURATION StreamConfig;
static unsigned short testPort;
static int testEncryption;

unsigned short LiGetNativeMicPort(void) { return testPort; }
int LiIsNativeMicAudioEncrypted(void) { return testEncryption; }

uint64_t PltGetMillis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

SOCKET createSocket(int family, int type, int protocol, bool nonBlocking) {
    int fd = socket(family, type, protocol);
    if (fd >= 0 && nonBlocking && fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        return INVALID_SOCKET;
    }
    return fd;
}
void closeSocket(SOCKET socket) { close(socket); }

void testConfigure(const char* address, unsigned short port, int encrypted, int requested) {
    struct sockaddr_in* endpoint = (struct sockaddr_in*)&RemoteAddr;
    memset(&RemoteAddr, 0, sizeof(RemoteAddr));
    endpoint->sin_family = AF_INET;
    inet_pton(AF_INET, address, &endpoint->sin_addr);
    testPort = port;
    testEncryption = encrypted;
    StreamConfig.requestNativeMic = requested;
    memcpy(StreamConfig.remoteInputAesKey, "0123456789abcdef", 16);
    memcpy(StreamConfig.remoteInputAesIv, "\x12\x34\x56\x78", 4);
}
