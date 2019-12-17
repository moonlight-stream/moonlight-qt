#include "vdpau.h"
#include <streaming/streamutils.h>

#include <SDL_syswm.h>

#define BAIL_ON_FAIL(status, something) if ((status) != VDP_STATUS_OK) { \
                                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, \
                                                        #something " failed: %d", (status)); \
                                            return false; \
                                        }

#define GET_PROC_ADDRESS(id, func) status = vdpauCtx->get_proc_address(m_Device, id, (void**)func); \
                                   BAIL_ON_FAIL(status, id)

const VdpRGBAFormat VDPAURenderer::k_OutputFormats8Bit[] = {
    VDP_RGBA_FORMAT_B8G8R8A8,
    VDP_RGBA_FORMAT_R8G8B8A8
};

const VdpRGBAFormat VDPAURenderer::k_OutputFormats10Bit[] = {
    VDP_RGBA_FORMAT_B10G10R10A2,
    VDP_RGBA_FORMAT_R10G10B10A2
};

VDPAURenderer::VDPAURenderer()
    : m_HwContext(nullptr),
      m_PresentationQueueTarget(0),
      m_PresentationQueue(0),
      m_VideoMixer(0),
      m_NextSurfaceIndex(0)
{
    SDL_zero(m_OutputSurface);
}

VDPAURenderer::~VDPAURenderer()
{
    if (m_PresentationQueue != 0) {
        m_VdpPresentationQueueDestroy(m_PresentationQueue);
    }

    if (m_VideoMixer != 0) {
        m_VdpVideoMixerDestroy(m_VideoMixer);
    }

    if (m_PresentationQueueTarget != 0) {
        m_VdpPresentationQueueTargetDestroy(m_PresentationQueueTarget);
    }

    for (int i = 0; i < OUTPUT_SURFACE_COUNT; i++) {
        if (m_OutputSurface[i] != 0) {
            m_VdpOutputSurfaceDestroy(m_OutputSurface[i]);
        }
    }

    // This must be done last as it frees VDPAU context required to call
    // the functions above.
    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }
}

bool VDPAURenderer::initialize(PDECODER_PARAMETERS params)
{
    int err;
    VdpStatus status;
    SDL_SysWMinfo info;

    m_VideoWidth = params->width;
    m_VideoHeight = params->height;

    err = av_hwdevice_ctx_create(&m_HwContext,
                                 AV_HWDEVICE_TYPE_VDPAU,
                                 nullptr, nullptr, 0);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create VDPAU context: %d",
                     err);
        return false;
    }

    AVHWDeviceContext* devCtx = (AVHWDeviceContext*)m_HwContext->data;
    AVVDPAUDeviceContext* vdpauCtx = (AVVDPAUDeviceContext*)devCtx->hwctx;
    m_Device = vdpauCtx->device;

    GET_PROC_ADDRESS(VDP_FUNC_ID_GET_ERROR_STRING, &m_VdpGetErrorString);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_DESTROY, &m_VdpPresentationQueueTargetDestroy);
    GET_PROC_ADDRESS(VDP_FUNC_ID_VIDEO_MIXER_CREATE, &m_VdpVideoMixerCreate);
    GET_PROC_ADDRESS(VDP_FUNC_ID_VIDEO_MIXER_DESTROY, &m_VdpVideoMixerDestroy);
    GET_PROC_ADDRESS(VDP_FUNC_ID_VIDEO_MIXER_RENDER, &m_VdpVideoMixerRender);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_CREATE, &m_VdpPresentationQueueCreate);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_DESTROY, &m_VdpPresentationQueueDestroy);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_DISPLAY, &m_VdpPresentationQueueDisplay);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_SET_BACKGROUND_COLOR, &m_VdpPresentationQueueSetBackgroundColor);
    GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_BLOCK_UNTIL_SURFACE_IDLE, &m_VdpPresentationQueueBlockUntilSurfaceIdle);
    GET_PROC_ADDRESS(VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, &m_VdpOutputSurfaceCreate);
    GET_PROC_ADDRESS(VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY, &m_VdpOutputSurfaceDestroy);
    GET_PROC_ADDRESS(VDP_FUNC_ID_OUTPUT_SURFACE_QUERY_CAPABILITIES, &m_VdpOutputSurfaceQueryCapabilities);
    GET_PROC_ADDRESS(VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS, &m_VdpVideoSurfaceGetParameters);
    GET_PROC_ADDRESS(VDP_FUNC_ID_GET_INFORMATION_STRING, &m_VdpGetInformationString);

    SDL_GetWindowSize(params->window, (int*)&m_DisplayWidth, (int*)&m_DisplayHeight);

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(params->window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    SDL_assert(info.subsystem == SDL_SYSWM_X11);

    if (info.subsystem == SDL_SYSWM_X11) {
        GET_PROC_ADDRESS(VDP_FUNC_ID_PRESENTATION_QUEUE_TARGET_CREATE_X11,
                         &m_VdpPresentationQueueTargetCreateX11);
        status = m_VdpPresentationQueueTargetCreateX11(m_Device,
                                                       info.info.x11.window,
                                                       &m_PresentationQueueTarget);
        if (status != VDP_STATUS_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VdpPresentationQueueTargetCreateX11() failed: %s",
                         m_VdpGetErrorString(status));
            return false;
        }
    }
    else if (info.subsystem == SDL_SYSWM_WAYLAND) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VDPAU backend does not currently support Wayland");
        return false;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unsupported VDPAU rendering subsystem: %d",
                     info.subsystem);
        return false;
    }

    const char* infoString;
    if (m_VdpGetInformationString(&infoString) == VDP_STATUS_OK) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Driver: %s",
                    infoString);
    }

    // Try our available output formats to find something the GPU supports
    bool foundFormat = false;
    for (int i = 0; i < OUTPUT_SURFACE_FORMAT_COUNT; i++) {
        VdpBool supported;
        uint32_t maxWidth, maxHeight;
        VdpRGBAFormat candidateFormat =
                params->videoFormat == VIDEO_FORMAT_H265_MAIN10 ?
                    k_OutputFormats10Bit[i] : k_OutputFormats8Bit[i];

        status = m_VdpOutputSurfaceQueryCapabilities(m_Device, candidateFormat,
                                                     &supported, &maxWidth, &maxHeight);
        if (status != VDP_STATUS_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VdpOutputSurfaceQueryCapabilities() failed: %s",
                         m_VdpGetErrorString(status));
            return false;
        }

        if (supported) {
            if (m_DisplayWidth <= maxWidth && m_DisplayHeight <= maxHeight) {
                m_OutputSurfaceFormat = candidateFormat;
                foundFormat = true;
                break;
            }
            else  {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Display size not within capabilities %dx%d vs %dx%d",
                            m_DisplayWidth, m_DisplayWidth,
                            maxWidth, maxHeight);
            }
        }
    }

    if (!foundFormat) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No compatible output surface format found!");
        return false;
    }

    // Create the output surfaces
    for (int i = 0; i < OUTPUT_SURFACE_COUNT; i++) {
        // It seems there's some lazy freeing going on or something in VDPAU
        // because we can get VDP_STATUS_RESOURCES, then wait a bit and it'll
        // complete without a problem.
        int tries = 1;
        do {
            status = m_VdpOutputSurfaceCreate(m_Device, m_OutputSurfaceFormat,
                                              m_DisplayWidth, m_DisplayHeight,
                                              &m_OutputSurface[i]);
            if (status != VDP_STATUS_OK) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "VdpOutputSurfaceCreate() try #%d: %s",
                            tries,
                            m_VdpGetErrorString(status));
                SDL_Delay(250);
            }
        } while (status == VDP_STATUS_RESOURCES && ++tries <= 10);

        if (status != VDP_STATUS_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VdpOutputSurfaceCreate() failed: %s",
                         m_VdpGetErrorString(status));
            return false;
        }
    }

    status = m_VdpPresentationQueueCreate(m_Device, m_PresentationQueueTarget,
                                          &m_PresentationQueue);
    if (status != VDP_STATUS_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VdpPresentationQueueCreate() failed: %s",
                     m_VdpGetErrorString(status));
        return false;
    }

    // Set the background to opaque black
    VdpColor color = {0.0, 0.0, 0.0, 1.0};
    m_VdpPresentationQueueSetBackgroundColor(m_PresentationQueue, &color);

    return true;
}

bool VDPAURenderer::prepareDecoderContext(AVCodecContext* context)
{
    context->hw_device_ctx = av_buffer_ref(m_HwContext);

    // Allow HEVC usage on VDPAU. This was disabled by FFmpeg due to
    // GL interop issues, but we use VDPAU for rendering so it's no issue.
    // https://github.com/FFmpeg/FFmpeg/commit/64ecb78b7179cab2dbdf835463104679dbb7c895
    context->hwaccel_flags |= AV_HWACCEL_FLAG_ALLOW_PROFILE_MISMATCH;

    // This flag is recommended due to hardware underreporting supported levels
    context->hwaccel_flags |= AV_HWACCEL_FLAG_IGNORE_LEVEL;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using VDPAU accelerated renderer");

    return true;
}

bool VDPAURenderer::needsTestFrame()
{
    // We need a test frame to see if this VDPAU driver
    // supports the profile used for streaming
    return true;
}

int VDPAURenderer::getDecoderColorspace()
{
    // VDPAU defaults to Rec 601.
    // https://http.download.nvidia.com/XFree86/vdpau/doxygen/html/group___vdp_video_mixer.html#ga65580813e9045d94b739ed2bb8b62b46
    //
    // AMD and Nvidia GPUs both correctly process Rec 601, so let's not try our luck using a non-default colorspace.
    return COLORSPACE_REC_601;
}

void VDPAURenderer::renderFrame(AVFrame* frame)
{
    VdpStatus status;
    VdpVideoSurface videoSurface = (VdpVideoSurface)(uintptr_t)frame->data[3];

    // This is safe without locking because this is always called on the main thread
    VdpOutputSurface chosenSurface = m_OutputSurface[m_NextSurfaceIndex];
    m_NextSurfaceIndex = (m_NextSurfaceIndex + 1) % OUTPUT_SURFACE_COUNT;

    // We need to create the mixer on the fly, because we don't know the dimensions
    // of our video surfaces in advance of decoding
    if (m_VideoMixer == 0) {
        VdpChromaType videoSurfaceChroma;
        uint32_t videoSurfaceWidth, videoSurfaceHeight;
        status = m_VdpVideoSurfaceGetParameters(videoSurface, &videoSurfaceChroma,
                                                &videoSurfaceWidth, &videoSurfaceHeight);
        if (status != VDP_STATUS_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VdpVideoSurfaceGetParameters() failed: %s",
                         m_VdpGetErrorString(status));
            return;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "VDPAU surface size: %dx%d",
                    videoSurfaceWidth, videoSurfaceHeight);

    #define PARAM_COUNT 3
        const VdpVideoMixerParameter params[PARAM_COUNT] = {
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
            VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT,
            VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
        };
        const void* const paramValues[PARAM_COUNT] = {
            &videoSurfaceWidth,
            &videoSurfaceHeight,
            &videoSurfaceChroma,
        };

        status = m_VdpVideoMixerCreate(m_Device, 0, nullptr,
                                       PARAM_COUNT, params, paramValues,
                                       &m_VideoMixer);
        if (status != VDP_STATUS_OK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VdpVideoMixerCreate() failed: %s",
                         m_VdpGetErrorString(status));
            return;
        }
    }

    // Wait for this frame to be off the screen
    VdpTime pts;
    m_VdpPresentationQueueBlockUntilSurfaceIdle(m_PresentationQueue, chosenSurface, &pts);

    VdpRect sourceRect, outputRect;

    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_VideoWidth;
    src.h = m_VideoHeight;
    dst.x = dst.y = 0;
    dst.w = m_DisplayWidth;
    dst.h = m_DisplayHeight;

    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    outputRect.x0 = dst.x;
    outputRect.x1 = dst.x + dst.w;
    outputRect.y0 = dst.y;
    outputRect.y1 = dst.y + dst.h;

    sourceRect.x0 = sourceRect.y0 = 0;
    sourceRect.x1 = m_VideoWidth;
    sourceRect.y1 = m_VideoHeight;

    // Render the next frame into the output surface
    status = m_VdpVideoMixerRender(m_VideoMixer,
                                   VDP_INVALID_HANDLE, nullptr,
                                   VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME,
                                   0, nullptr,
                                   videoSurface,
                                   0, nullptr,
                                   &sourceRect,
                                   chosenSurface,
                                   nullptr,
                                   &outputRect,
                                   0,
                                   nullptr);
    if (status != VDP_STATUS_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VdpVideoMixerRender() failed: %s",
                     m_VdpGetErrorString(status));
        return;
    }

    // Queue the frame for display immediately
    status = m_VdpPresentationQueueDisplay(m_PresentationQueue, chosenSurface, 0, 0, 0);
    if (status != VDP_STATUS_OK) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VdpPresentationQueueDisplay() failed: %s",
                     m_VdpGetErrorString(status));
        return;
    }
}
