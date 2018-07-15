#include "vt.h"

#include <Limelight.h>

VTRenderer::VTRenderer()
    : m_HwContext(nullptr)
{

}

VTRenderer::~VTRenderer()
{
    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }
}

bool VTRenderer::prepareDecoderContext(AVCodecContext* context)
{
    context->hw_device_ctx = av_buffer_ref(m_HwContext);
    return true;
}

bool VTRenderer::initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height)
{
    int err;

    if (videoFormat & VIDEO_FORMAT_MASK_H264) {
        // Prior to 10.13, we'll just assume everything has
        // H.264 support and fail open to allow VT decode.
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
        if (__builtin_available(macOS 10.13, *)) {
            if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_H264)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HW accelerated H.264 decode via VT");
                return false;
            }
        }
        else
#endif
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Assuming H.264 HW decode on < macOS 10.13");
        }
    }
    else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300
        if (__builtin_available(macOS 10.13, *)) {
            if (!VTIsHardwareDecodeSupported(kCMVideoCodecType_HEVC)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "No HW accelerated HEVC decode via VT");
                return false;
            }
        }
        else
#endif
        {
            // Fail closed for HEVC if we're not on 10.13+
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "No HEVC support on < macOS 10.13");
            return false;
        }
    }

    err = av_hwdevice_ctx_create(&m_HwContext,
                                 AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
                                 nullptr,
                                 nullptr,
                                 0);
    if (err < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "av_hwdevice_ctx_create() failed for VT decoder: %d",
                    err);
        return false;
    }

    return false; // true to test VT
}

void VTRenderer::renderFrame(AVFrame* frame)
{
    CVPixelBufferRef pixBuf = reinterpret_cast<CVPixelBufferRef>(frame->data[3]);
}
