#pragma once

#include <mach/mach_time.h>

#include <AudioToolbox/AudioToolbox.h>
#include <QtGlobal>
#include <SDL.h>

#ifndef NDEBUG
#define COREAUDIO_DEBUG
# define DEBUG_TRACE(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__)
#else
# define DEBUG_TRACE(...)
#endif

static void CA_LogError(OSStatus error, const char *fmt, ...)
{
    char errorString[20];

    // See if it appears to be a 4-char-code
    *(uint32_t *)(errorString + 1) = CFSwapInt32HostToBig(error);
    if (isprint(errorString[1]) && isprint(errorString[2]) &&
        isprint(errorString[3]) && isprint(errorString[4])) {
        errorString[0] = errorString[5] = '\'';
        errorString[6] = '\0';
    }
    else {
        // No, format it as an integer
        snprintf(errorString, sizeof(errorString), "%d", (int)error);
    }

    char logBuffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuffer, sizeof(logBuffer), fmt, args);
    va_end(args);

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CoreAudio Error: %s (%s)\n", logBuffer, errorString);
}

static void CA_FourCC(uint32_t value, char *outFormatIDStr)
{
    uint32_t formatID = CFSwapInt32HostToBig(value);
    bcopy(&formatID, outFormatIDStr, 4);
    outFormatIDStr[4] = '\0';
}

// based on mpv ca_print_asbd()
static void CA_PrintASBD(const char *description, const AudioStreamBasicDescription *asbd)
{
    char formatIDStr[5];
    CA_FourCC(asbd->mFormatID, formatIDStr);

    uint32_t flags  = asbd->mFormatFlags;
    DEBUG_TRACE(
        "%s %7.1fHz %" PRIu32 "bit %s "
        "[%" PRIu32 "bpp][%" PRIu32 "fpp]"
        "[%" PRIu32 "bpf][%" PRIu32 "ch] "
        "%s %s %s%s%s%s\n",
        description, asbd->mSampleRate, asbd->mBitsPerChannel, formatIDStr,
        asbd->mBytesPerPacket, asbd->mFramesPerPacket,
        asbd->mBytesPerFrame, asbd->mChannelsPerFrame,
        (flags & kAudioFormatFlagIsFloat) ? "float" : "int",
        (flags & kAudioFormatFlagIsBigEndian) ? "BE" : "LE",
        (flags & kAudioFormatFlagIsFloat) ? ""
            : ((flags & kAudioFormatFlagIsSignedInteger) ? "S" : "U"),
        (flags & kAudioFormatFlagIsPacked) ? " packed" : "",
        (flags & kAudioFormatFlagIsAlignedHigh) ? " aligned" : "",
        (flags & kAudioFormatFlagIsNonInterleaved) ? " non-interleaved" : " interleaved");
}

// classic hex dump
static void CA_HexDump(const float *buffer, size_t length)
{
    const uint8_t *bytePtr = (const uint8_t *)buffer;
    size_t bytesToPrint = length * sizeof(float);

    // Print 32 bytes per line
    for (size_t i = 0; i < bytesToPrint; i += 32) {
        printf("%08lx  ", (unsigned long)(bytePtr + i));

        // Print the hex values (32 bytes)
        for (size_t j = 0; j < 32 && (i + j) < bytesToPrint; ++j) {
            printf("%02x ", bytePtr[i + j]);
            if (j == 15) printf(" ");
        }

        printf(" |");

        for (size_t j = 0; j < 32 && (i + j) < bytesToPrint; ++j) {
            uint8_t byte = bytePtr[i + j];
            if (byte >= 32 && byte <= 126)
                printf("%c", byte);
            else
                printf(".");
        }

        printf("|\n");
    }
}
