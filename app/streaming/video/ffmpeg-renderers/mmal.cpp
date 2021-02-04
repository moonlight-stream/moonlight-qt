#include "mmal.h"

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <Limelight.h>

#include <SDL_syswm.h>

MmalRenderer::MmalRenderer()
    : m_Renderer(nullptr),
      m_InputPort(nullptr),
      m_BackgroundRenderer(nullptr),
      m_Window(nullptr),
      m_VideoWidth(0),
      m_VideoHeight(0),
      m_LastWindowPosX(-1),
      m_LastWindowPosY(-1)
{
}

MmalRenderer::~MmalRenderer()
{
    if (m_InputPort != nullptr) {
        mmal_port_disable(m_InputPort);
    }

    if (m_Renderer != nullptr) {
        mmal_component_destroy(m_Renderer);
    }

    if (m_BackgroundRenderer != nullptr) {
        SDL_DestroyRenderer(m_BackgroundRenderer);
    }
}

bool MmalRenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary** options)
{
    // FFmpeg defaults this to 10 which is too large to fit in the default 64 MB VRAM split.
    // Reducing to 2 seems to work fine for our bitstreams (max of 1 buffered frame needed).
    av_dict_set_int(options, "extra_buffers", 2, 0);

    // MMAL seems to dislike certain initial width and height values, but it seems okay
    // with getting zero for the width and height. We'll zero them all the time to be safe.
    context->width = 0;
    context->height = 0;

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using MMAL renderer");

    return true;
}

void MmalRenderer::updateDisplayRegion()
{
    MMAL_STATUS_T status;
    int currentPosX, currentPosY;
    MMAL_DISPLAYREGION_T dr;

    dr.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    dr.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    dr.set = MMAL_DISPLAY_SET_DEST_RECT;

    SDL_GetWindowPosition(m_Window, &currentPosX, &currentPosY);

    if ((SDL_GetWindowFlags(m_Window) & SDL_WINDOW_INPUT_FOCUS) == 0) {
        dr.dest_rect.x = 0;
        dr.dest_rect.y = 0;
        dr.dest_rect.width = 0;
        dr.dest_rect.height = 0;

        // Force a re-evaluation next time
        m_LastWindowPosX = -1;
        m_LastWindowPosY = -1;
    }
    else if (m_LastWindowPosX != currentPosX || m_LastWindowPosY != currentPosY) {
        SDL_Rect src, dst;
        src.x = src.y = 0;
        src.w = m_VideoWidth;
        src.h = m_VideoHeight;
        dst.x = dst.y = 0;
        SDL_GetWindowSize(m_Window, &dst.w, &dst.h);

        StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

        dr.dest_rect.x = currentPosX + dst.x;
        dr.dest_rect.y = currentPosY + dst.y;
        dr.dest_rect.width = dst.w;
        dr.dest_rect.height = dst.h;

        m_LastWindowPosX = currentPosX;
        m_LastWindowPosY = currentPosY;
    }
    else {
        // Nothing to do
        return;
    }

    status = mmal_port_parameter_set(m_InputPort, &dr.hdr);
    if (status != MMAL_SUCCESS) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "mmal_port_parameter_set() failed: %x (%s)",
                    status, mmal_status_to_string(status));
    }
}

bool MmalRenderer::initialize(PDECODER_PARAMETERS params)
{
    MMAL_STATUS_T status;

    m_Window = params->window;
    m_VideoWidth = params->width;
    m_VideoHeight = params->height;

    // Clear the background if possible
    setupBackground(params);

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &m_Renderer);
    if (status != MMAL_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmal_component_create() failed: %x (%s)",
                     status, mmal_status_to_string(status));
        return false;
    }

    m_InputPort = m_Renderer->input[0];

    m_InputPort->format->encoding = MMAL_ENCODING_OPAQUE;
    m_InputPort->format->es->video.width = params->width;
    m_InputPort->format->es->video.height = params->height;
    m_InputPort->format->es->video.crop.x = 0;
    m_InputPort->format->es->video.crop.y = 0;
    m_InputPort->format->es->video.crop.width = params->width;
    m_InputPort->format->es->video.crop.height = params->height;
    status = mmal_port_format_commit(m_InputPort);
    if (status != MMAL_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmal_port_format_commit() failed: %x (%s)",
                     status, mmal_status_to_string(status));
        return false;
    }

    status = mmal_component_enable(m_Renderer);
    if (status != MMAL_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmal_component_enable() failed: %x (%s)",
                     status, mmal_status_to_string(status));
        return false;
    }

    {
        MMAL_DISPLAYREGION_T dr = {};

        dr.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        dr.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

        dr.set |= MMAL_DISPLAY_SET_FULLSCREEN;
        dr.fullscreen = MMAL_FALSE;

        dr.set |= MMAL_DISPLAY_SET_MODE;
        dr.mode = MMAL_DISPLAY_MODE_LETTERBOX;

        dr.set |= MMAL_DISPLAY_SET_NOASPECT;
        dr.noaspect = MMAL_TRUE;

        dr.set |= MMAL_DISPLAY_SET_SRC_RECT;
        dr.src_rect.x = 0;
        dr.src_rect.y = 0;
        dr.src_rect.width = params->width;
        dr.src_rect.height = params->height;

        status = mmal_port_parameter_set(m_InputPort, &dr.hdr);
        if (status != MMAL_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "mmal_port_parameter_set() failed: %x (%s)",
                         status, mmal_status_to_string(status));
            return false;
        }

        // Set the destination display region
        updateDisplayRegion();
    }

    status = mmal_port_enable(m_InputPort, InputPortCallback);
    if (status != MMAL_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmal_port_enable() failed: %x (%s)",
                     status, mmal_status_to_string(status));
        return false;
    }

    return true;
}

void MmalRenderer::setupBackground(PDECODER_PARAMETERS params)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(params->window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return;
    }

    // On X11, we can safely create a renderer and draw a black background.
    // Due to conflicts with Qt, it's unsafe to do this for KMSDRM.
    if (info.subsystem == SDL_SYSWM_X11) {
        m_BackgroundRenderer = SDL_CreateRenderer(params->window, -1, SDL_RENDERER_SOFTWARE);
        if (m_BackgroundRenderer == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "SDL_CreateRenderer() failed: %s",
                        SDL_GetError());
            return;
        }

        SDL_SetRenderDrawColor(m_BackgroundRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(m_BackgroundRenderer);
        SDL_RenderPresent(m_BackgroundRenderer);
    }
}

void MmalRenderer::InputPortCallback(MMAL_PORT_T*, MMAL_BUFFER_HEADER_T* buffer)
{
    mmal_buffer_header_release(buffer);
}

enum AVPixelFormat MmalRenderer::getPreferredPixelFormat(int)
{
    // Opaque MMAL buffers
    return AV_PIX_FMT_MMAL;
}

int MmalRenderer::getRendererAttributes()
{
    // This renderer maxes out at 1080p
    return RENDERER_ATTRIBUTE_1080P_MAX;
}

bool MmalRenderer::needsTestFrame()
{
    // We won't be able to decode if the GPU memory is 64 MB or lower,
    // so we must test before allowing the decoder to be used.
    return true;
}

void MmalRenderer::renderFrame(AVFrame* frame)
{
    if (frame == nullptr) {
        // End of stream - nothing to do for us
        return;
    }

    MMAL_BUFFER_HEADER_T* buffer = (MMAL_BUFFER_HEADER_T*)frame->data[3];
    MMAL_STATUS_T status;

    // Update the destination display region in case the window moved
    updateDisplayRegion();

    status = mmal_port_send_buffer(m_InputPort, buffer);
    if (status != MMAL_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "mmal_port_send_buffer() failed: %x (%s)",
                     status, mmal_status_to_string(status));
    }
    else {
        // Prevent the buffer from being freed during av_frame_free()
        // until rendering is complete. The reference is dropped in
        // InputPortCallback().
        mmal_buffer_header_acquire(buffer);
    }
}
