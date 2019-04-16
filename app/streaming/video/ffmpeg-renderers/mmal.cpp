#include "mmal.h"

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <Limelight.h>

MmalRenderer::MmalRenderer()
    : m_Renderer(nullptr),
      m_InputPort(nullptr)
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
}

bool MmalRenderer::prepareDecoderContext(AVCodecContext*)
{
    /* Nothing to do */

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using MMAL renderer");

    return true;
}

bool MmalRenderer::initialize(PDECODER_PARAMETERS params)
{
    MMAL_STATUS_T status;

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
        MMAL_DISPLAYREGION_T dr;

        dr.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
        dr.hdr.size = sizeof(MMAL_DISPLAYREGION_T);

        dr.set = MMAL_DISPLAY_SET_LAYER;
        dr.layer = 128;

        dr.set |= MMAL_DISPLAY_SET_ALPHA;
        dr.alpha = 255;

        dr.set |= MMAL_DISPLAY_SET_FULLSCREEN;
        dr.fullscreen = (SDL_GetWindowFlags(params->window) & SDL_WINDOW_FULLSCREEN) != 0;

        {
            SDL_Rect src, dst;
            src.x = src.y = 0;
            src.w = params->width;
            src.h = params->height;
            dst.x = dst.y = 0;
            SDL_GetWindowSize(params->window, &dst.w, &dst.h);

            StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

            dr.set |= MMAL_DISPLAY_SET_DEST_RECT;
            dr.dest_rect.x = dst.x;
            dr.dest_rect.y = dst.y;
            dr.dest_rect.width = dst.w;
            dr.dest_rect.height = dst.h;
        }
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

void MmalRenderer::InputPortCallback(MMAL_PORT_T*, MMAL_BUFFER_HEADER_T* buffer)
{
    mmal_buffer_header_release(buffer);
}

enum AVPixelFormat MmalRenderer::getPreferredPixelFormat(int videoFormat)
{
    // Opaque MMAL buffers
    SDL_assert(videoFormat == VIDEO_FORMAT_H264);
    return AV_PIX_FMT_MMAL;
}

void MmalRenderer::renderFrame(AVFrame* frame)
{
    MMAL_BUFFER_HEADER_T* buffer = (MMAL_BUFFER_HEADER_T*)frame->data[3];
    MMAL_STATUS_T status;

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
