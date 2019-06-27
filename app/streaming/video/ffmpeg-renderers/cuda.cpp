#include "cuda.h"

CUDARenderer::CUDARenderer()
    : m_HwContext(nullptr)
{

}

CUDARenderer::~CUDARenderer()
{
    if (m_HwContext != nullptr) {
        av_buffer_unref(&m_HwContext);
    }
}

bool CUDARenderer::initialize(PDECODER_PARAMETERS)
{
    int err;

    err = av_hwdevice_ctx_create(&m_HwContext, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    if (err != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "av_hwdevice_ctx_create(CUDA) failed: %d",
                     err);
        return false;
    }

    return true;
}

bool CUDARenderer::prepareDecoderContext(AVCodecContext* context)
{
    context->hw_device_ctx = av_buffer_ref(m_HwContext);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using CUDA accelerated decoder");

    return true;
}

void CUDARenderer::renderFrame(AVFrame*)
{
    // We only support indirect rendering
    SDL_assert(false);
}

bool CUDARenderer::needsTestFrame()
{
    return true;
}

bool CUDARenderer::isDirectRenderingSupported()
{
    // We only support rendering via SDL read-back
    return false;
}

