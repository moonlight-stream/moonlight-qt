// This compilation unit contains the implementations of libplacebo header-only libraries.
// These must be compiled as C code, so they cannot be placed inside plvk.cpp.

#define PL_LIBAV_IMPLEMENTATION 1
#include <libplacebo/utils/libav.h>

// Provide a dummy implementation of av_stream_get_side_data() to avoid having to link with libavformat
uint8_t *av_stream_get_side_data(const AVStream *stream, enum AVPacketSideDataType type, size_t *size)
{
    (void)stream;
    (void)type;
    (void)size;
    return NULL;
}
