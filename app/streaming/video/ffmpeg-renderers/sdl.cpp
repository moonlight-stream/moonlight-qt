#include "renderer.h"

#include <Limelight.h>

SdlRenderer::SdlRenderer()
    : m_Renderer(nullptr),
      m_Texture(nullptr)
{

}

SdlRenderer::~SdlRenderer()
{
    if (m_Texture != nullptr) {
        SDL_DestroyTexture(m_Texture);
    }

    if (m_Renderer != nullptr) {
        SDL_DestroyRenderer(m_Renderer);
    }
}

bool SdlRenderer::prepareDecoderContext(AVCodecContext*)
{
    /* Nothing to do */
    return true;
}

bool SdlRenderer::initialize(SDL_Window* window,
                             int videoFormat,
                             int width,
                             int height)
{
    // NOTE: HEVC currently uses only 1 slice regardless of what
    // we provide in CAPABILITY_SLICES_PER_FRAME(), so we should
    // never use it for software decoding (unless common-c starts
    // respecting it for HEVC).
    if (videoFormat != VIDEO_FORMAT_H264) {
        return false;
    }

    m_Renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_Renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    // The window may be smaller than the stream size, so ensure our
    // logical rendering surface size is equal to the stream size
    SDL_RenderSetLogicalSize(m_Renderer, width, height);

    // Draw a black frame until the video stream starts rendering
    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(m_Renderer);
    SDL_RenderPresent(m_Renderer);

    m_Texture = SDL_CreateTexture(m_Renderer,
                                  SDL_PIXELFORMAT_YV12,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  width,
                                  height);
    if (!m_Texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    return true;
}

void SdlRenderer::renderFrame(AVFrame* frame)
{
    SDL_UpdateYUVTexture(m_Texture, nullptr,
                         frame->data[0],
                         frame->linesize[0],
                         frame->data[1],
                         frame->linesize[1],
                         frame->data[2],
                         frame->linesize[2]);

    // Done with the frame now
    av_frame_free(&frame);

    SDL_RenderClear(m_Renderer);
    SDL_RenderCopy(m_Renderer, m_Texture, nullptr, nullptr);
    SDL_RenderPresent(m_Renderer);
}
