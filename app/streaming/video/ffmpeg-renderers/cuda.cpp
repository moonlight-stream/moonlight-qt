#include "cuda.h"

#include <ffnvcodec/dynlink_loader.h>

#include <SDL_opengl.h>

extern "C" {
    #include <libavutil/hwcontext_cuda.h>
}

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

bool CUDARenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary**)
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

bool CUDARenderer::copyCudaFrameToBoundTexture(AVFrame* frame)
{
    static CudaFunctions* funcs;
    CUresult err;
    AVCUDADeviceContext* devCtx = (AVCUDADeviceContext*)(((AVHWFramesContext*)frame->hw_frames_ctx->data)->device_ctx->hwctx);
    bool ret = false;

    if (!funcs) {
        // One-time init of CUDA library
        cuda_load_functions(&funcs, nullptr);
        if (!funcs) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize CUDA library");
            return false;
        }
    }

    SDL_assert(frame->format == AV_PIX_FMT_CUDA);

    // Push FFmpeg's CUDA context to use for our CUDA operations
    err = funcs->cuCtxPushCurrent(devCtx->cuda_ctx);
    if (err != CUDA_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cuCtxPushCurrent() failed: %d", err);
        return false;
    }

    // NV12 has 2 planes
    for (int i = 0; i < 2; i++) {
        CUgraphicsResource cudaResource;
        CUarray cudaArray;
        GLint tex;

        // Get the ID of this plane's texture
        glActiveTexture(GL_TEXTURE0 + i);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);

        // Register it with CUDA
        err = funcs->cuGraphicsGLRegisterImage(&cudaResource, tex, GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD);
        if (err != CUDA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cuGraphicsGLRegisterImage() failed: %d", err);
            goto Exit;
        }

        // Map it to allow us to use it as a copy destination
        err = funcs->cuGraphicsMapResources(1, &cudaResource, devCtx->stream);
        if (err != CUDA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cuGraphicsMapResources() failed: %d", err);
            funcs->cuGraphicsUnregisterResource(cudaResource);
            goto Exit;
        }

        // Get a pointer to the mapped array
        err = funcs->cuGraphicsSubResourceGetMappedArray(&cudaArray, cudaResource, 0, 0);
        if (err != CUDA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cuGraphicsSubResourceGetMappedArray() failed: %d", err);
            funcs->cuGraphicsUnmapResources(1, &cudaResource, devCtx->stream);
            funcs->cuGraphicsUnregisterResource(cudaResource);
            goto Exit;
        }

        CUDA_MEMCPY2D cu2d = {
            .srcMemoryType = CU_MEMORYTYPE_DEVICE,
            .srcDevice = (CUdeviceptr)frame->data[i],
            .srcPitch = (size_t)frame->linesize[i],
            .dstMemoryType = CU_MEMORYTYPE_ARRAY,
            .dstArray = cudaArray,
            .dstPitch = (size_t)frame->width >> i,
            .WidthInBytes = (size_t)frame->width,
            .Height = (size_t)frame->height >> i
        };

        // Do the copy
        err = funcs->cuMemcpy2D(&cu2d);
        if (err != CUDA_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "cuMemcpy2D() failed: %d", err);
            funcs->cuGraphicsUnmapResources(1, &cudaResource, devCtx->stream);
            funcs->cuGraphicsUnregisterResource(cudaResource);
            goto Exit;
        }

        funcs->cuGraphicsUnmapResources(1, &cudaResource, devCtx->stream);
        funcs->cuGraphicsUnregisterResource(cudaResource);
    }

    ret = true;

Exit:
    {
        CUcontext dummy;
        funcs->cuCtxPopCurrent(&dummy);
    }
    return ret;
}

