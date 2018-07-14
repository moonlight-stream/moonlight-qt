#include <Initguid.h>
#include "dxva2.h"

#include <Limelight.h>

DEFINE_GUID(DXVADDI_Intel_ModeH264_E, 0x604F8E68,0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);

#define SAFE_COM_RELEASE(x) if (x) { (x)->Release(); }

DXVA2Renderer::DXVA2Renderer() :
    m_FrameIndex(0),
    m_SurfacesUsed(0),
    m_SdlRenderer(nullptr),
    m_DecService(nullptr),
    m_Decoder(nullptr),
    m_Pool(nullptr),
    m_Device(nullptr),
    m_RenderTarget(nullptr),
    m_ProcService(nullptr),
    m_Processor(nullptr)
{
    RtlZeroMemory(m_DecSurfaces, sizeof(m_DecSurfaces));
    RtlZeroMemory(&m_DXVAContext, sizeof(m_DXVAContext));
}

DXVA2Renderer::~DXVA2Renderer()
{
    SAFE_COM_RELEASE(m_DecService);
    SAFE_COM_RELEASE(m_Decoder);
    SAFE_COM_RELEASE(m_Device);
    SAFE_COM_RELEASE(m_RenderTarget);
    SAFE_COM_RELEASE(m_ProcService);
    SAFE_COM_RELEASE(m_Processor);

    for (int i = 0; i < ARRAYSIZE(m_DecSurfaces); i++) {
        SAFE_COM_RELEASE(m_DecSurfaces[i]);
    }

    if (m_Pool != nullptr) {
        av_buffer_pool_uninit(&m_Pool);
    }

    if (m_SdlRenderer != nullptr) {
        SDL_DestroyRenderer(m_SdlRenderer);
    }
}

void DXVA2Renderer::ffPoolDummyDelete(void*, uint8_t*)
{
    /* Do nothing */
}

AVBufferRef* DXVA2Renderer::ffPoolAlloc(void* opaque, int)
{
    DXVA2Renderer* me = reinterpret_cast<DXVA2Renderer*>(opaque);

    if (me->m_SurfacesUsed < ARRAYSIZE(me->m_DecSurfaces)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Using buffer: %d", me->m_SurfacesUsed);
        return av_buffer_create((uint8_t*)me->m_DecSurfaces[me->m_SurfacesUsed++],
                                sizeof(*me->m_DecSurfaces), ffPoolDummyDelete, 0, 0);
    }

    return NULL;
}

bool DXVA2Renderer::prepareDecoderContext(AVCodecContext* context)
{
    // m_DXVAContext.workaround and report_id already initialized elsewhere
    m_DXVAContext.decoder = m_Decoder;
    m_DXVAContext.cfg = &m_Config;
    m_DXVAContext.surface = m_DecSurfaces;
    m_DXVAContext.surface_count = ARRAYSIZE(m_DecSurfaces);

    context->hwaccel_context = &m_DXVAContext;

    context->get_format = ffGetFormat;
    context->get_buffer2 = ffGetBuffer2;

    context->opaque = this;

    m_Pool = av_buffer_pool_init2(ARRAYSIZE(m_DecSurfaces), this, ffPoolAlloc, nullptr);
    if (!m_Pool) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed create buffer pool");
        return false;
    }

    return true;
}

int DXVA2Renderer::ffGetBuffer2(AVCodecContext* context, AVFrame* frame, int)
{
    DXVA2Renderer* me = reinterpret_cast<DXVA2Renderer*>(context->opaque);

    frame->buf[0] = av_buffer_pool_get(me->m_Pool);
    if (!frame->buf[0]) {
        return AVERROR(ENOMEM);
    }

    frame->data[3] = frame->buf[0]->data;
    frame->format = AV_PIX_FMT_DXVA2_VLD;
    frame->width = me->m_Width;
    frame->height = me->m_Height;

    return 0;
}

enum AVPixelFormat DXVA2Renderer::ffGetFormat(AVCodecContext*,
                                              const enum AVPixelFormat* pixFmts)
{
    const enum AVPixelFormat *p;

    for (p = pixFmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_DXVA2_VLD)
            return *p;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to find DXVA2 HW surface format");
    return AV_PIX_FMT_NONE;
}

bool DXVA2Renderer::initializeDecoder()
{
    HRESULT hr;

    hr = DXVA2CreateVideoService(m_Device, IID_IDirectXVideoDecoderService,
                                 reinterpret_cast<void**>(&m_DecService));
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2CreateVideoService(IID_IDirectXVideoDecoderService) failed: %x",
                     hr);
        return false;
    }

    GUID* guids;
    GUID chosenDeviceGuid;
    UINT guidCount;
    hr = m_DecService->GetDecoderDeviceGuids(&guidCount, &guids);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetDecoderDeviceGuids() failed: %x",
                     hr);
        return false;
    }

    UINT i;
    for (i = 0; i < guidCount; i++) {
        if (m_VideoFormat == VIDEO_FORMAT_H264) {
            if (IsEqualGUID(guids[i], DXVA2_ModeH264_E) ||
                    IsEqualGUID(guids[i], DXVA2_ModeH264_F)) {
                chosenDeviceGuid = guids[i];
                break;
            }
            else if (IsEqualGUID(guids[i], DXVADDI_Intel_ModeH264_E)) {
                chosenDeviceGuid = guids[i];
                m_DXVAContext.workaround |= FF_DXVA2_WORKAROUND_INTEL_CLEARVIDEO;
                break;
            }
        }
        else if (m_VideoFormat == VIDEO_FORMAT_H265) {
            if (IsEqualGUID(guids[i], DXVA2_ModeHEVC_VLD_Main)) {
                chosenDeviceGuid = guids[i];
                break;
            }
        }
        else if (m_VideoFormat == VIDEO_FORMAT_H265_MAIN10) {
            if (IsEqualGUID(guids[i], DXVA2_ModeHEVC_VLD_Main10)) {
                chosenDeviceGuid = guids[i];
                break;
            }
        }
    }

    CoTaskMemFree(guids);

    if (i == guidCount) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No matching decoder device GUIDs");
        return false;
    }

    DXVA2_ConfigPictureDecode* configs;
    UINT configCount;
    hr = m_DecService->GetDecoderConfigurations(chosenDeviceGuid, &m_Desc, nullptr, &configCount, &configs);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetDecoderConfigurations() failed: %x",
                     hr);
        return false;
    }

    for (i = 0; i < configCount; i++) {
        if ((configs[i].ConfigBitstreamRaw == 1 || configs[i].ConfigBitstreamRaw == 2) &&
                IsEqualGUID(configs[i].guidConfigBitstreamEncryption, DXVA2_NoEncrypt)) {
            m_Config = configs[i];
            break;
        }
    }

    CoTaskMemFree(configs);

    if (i == configCount) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "No matching decoder configurations");
        return false;
    }

    int alignment;

    // HEVC using DXVA requires 128B alignment
    if (m_VideoFormat & VIDEO_FORMAT_MASK_H265) {
        alignment = 128;
    }
    else {
        alignment = 16;
    }

    hr = m_DecService->CreateSurface(FFALIGN(m_Width, alignment),
                                     FFALIGN(m_Height, alignment),
                                     ARRAYSIZE(m_DecSurfaces) - 1,
                                     m_Desc.Format,
                                     D3DPOOL_DEFAULT,
                                     0,
                                     DXVA2_VideoDecoderRenderTarget,
                                     m_DecSurfaces,
                                     nullptr);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateSurface() failed: %x",
                     hr);
        return false;
    }

    hr = m_DecService->CreateVideoDecoder(chosenDeviceGuid, &m_Desc, &m_Config,
                                          m_DecSurfaces, ARRAYSIZE(m_DecSurfaces),
                                          &m_Decoder);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateVideoDecoder() failed: %x",
                     hr);
        return false;
    }

    return true;
}

bool DXVA2Renderer::initializeRenderer()
{
    HRESULT hr;

    hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_RenderTarget);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetBackBuffer() failed: %x",
                     hr);
        return false;
    }

    D3DSURFACE_DESC renderTargetDesc;
    m_RenderTarget->GetDesc(&renderTargetDesc);

    hr = DXVA2CreateVideoService(m_Device, IID_IDirectXVideoProcessorService,
                                 reinterpret_cast<void**>(&m_ProcService));

    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2CreateVideoService(IID_IDirectXVideoProcessorService) failed: %x",
                     hr);
        return false;
    }

    UINT guidCount;
    GUID* guids;
    hr = m_ProcService->GetVideoProcessorDeviceGuids(&m_Desc, &guidCount, &guids);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetVideoProcessorDeviceGuids() failed: %x",
                     hr);
        return false;
    }

    UINT i;
    for (i = 0; i < guidCount; i++) {
        DXVA2_VideoProcessorCaps caps;
        hr = m_ProcService->GetVideoProcessorCaps(guids[i], &m_Desc, renderTargetDesc.Format, &caps);
        if (SUCCEEDED(hr)) {
            if (!(caps.DeviceCaps & DXVA2_VPDev_HardwareDevice)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Device %d is not hardware: %x",
                            i,
                            caps.DeviceCaps);
                continue;
            }
            else if (!(caps.VideoProcessorOperations & DXVA2_VideoProcess_YUV2RGB) &&
                     !(caps.VideoProcessorOperations & DXVA2_VideoProcess_YUV2RGBExtended)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Device %d can't convert YUV2RGB: %x",
                            i,
                            caps.DeviceCaps);
                continue;
            }

            m_ProcService->GetProcAmpRange(guids[i], &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Brightness, &m_BrightnessRange);
            m_ProcService->GetProcAmpRange(guids[i], &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Contrast, &m_ContrastRange);
            m_ProcService->GetProcAmpRange(guids[i], &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Hue, &m_HueRange);
            m_ProcService->GetProcAmpRange(guids[i], &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Saturation, &m_SaturationRange);

            hr = m_ProcService->CreateVideoProcessor(guids[i], &m_Desc, renderTargetDesc.Format, 0, &m_Processor);
            if (FAILED(hr)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "CreateVideoProcessor() failed for GUID %d: %x",
                            i,
                            hr);
                continue;
            }

            break;
        }
        else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "GetVideoProcessorCaps() failed for GUID %d: %x",
                        i,
                        hr);
        }
    }

    CoTaskMemFree(guids);

    if (i == guidCount) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to find a usable DXVA2 processor");
        return false;
    }

    return true;
}

bool DXVA2Renderer::initialize(SDL_Window* window, int videoFormat, int width, int height)
{
    m_VideoFormat = videoFormat;
    m_Width = width;
    m_Height = height;

    m_SdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_SdlRenderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_CreateRenderer() failed: %s",
                     SDL_GetError());
        return false;
    }

    m_Device = SDL_RenderGetD3D9Device(m_SdlRenderer);

    RtlZeroMemory(&m_Desc, sizeof(m_Desc));
    m_Desc.SampleWidth = m_Width;
    m_Desc.SampleHeight = m_Height;
    m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_ProgressiveChroma;
    m_Desc.SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
    m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
    m_Desc.SampleFormat.VideoLighting = DXVA2_VideoLighting_dim;
    m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
    m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
    m_Desc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    m_Desc.Format = (D3DFORMAT)MAKEFOURCC('N','V','1','2');

    if (!initializeDecoder()) {
        return false;
    }

    if (!initializeRenderer()) {
        return false;
    }

    return true;
}

void DXVA2Renderer::renderFrame(AVFrame* frame)
{
    IDirect3DSurface9* surface = reinterpret_cast<IDirect3DSurface9*>(frame->data[3]);
    HRESULT hr;

    hr = m_Device->TestCooperativeLevel();
    switch (hr) {
    case D3D_OK:
        break;
    case D3DERR_DEVICELOST:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D device lost!");
        return;
    case D3DERR_DEVICENOTRESET:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D device not reset!");
        return;
    default:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unknown D3D error: %x",
                     hr);
        return;
    }

    DXVA2_VideoSample sample = {};
    sample.Start = m_FrameIndex;
    sample.End = m_FrameIndex + 1;
    sample.SrcSurface = surface;
    sample.SrcRect.right = m_Desc.SampleWidth;
    sample.SrcRect.bottom = m_Desc.SampleHeight;
    sample.DstRect = sample.SrcRect;
    sample.SampleFormat = m_Desc.SampleFormat;
    sample.PlanarAlpha = DXVA2_Fixed32OpaqueAlpha();

    DXVA2_VideoProcessBltParams bltParams = {};

    bltParams.TargetFrame = m_FrameIndex++;
    bltParams.TargetRect.right = m_Desc.SampleWidth;
    bltParams.TargetRect.bottom = m_Desc.SampleHeight;
    bltParams.BackgroundColor.Y = 0x1000;
    bltParams.BackgroundColor.Cb = 0x8000;
    bltParams.BackgroundColor.Cr = 0x8000;
    bltParams.BackgroundColor.Alpha = 0xFFFF;

    bltParams.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

    bltParams.ProcAmpValues.Brightness = m_BrightnessRange.DefaultValue;
    bltParams.ProcAmpValues.Contrast = m_ContrastRange.DefaultValue;
    bltParams.ProcAmpValues.Hue = m_HueRange.DefaultValue;
    bltParams.ProcAmpValues.Saturation = m_SaturationRange.DefaultValue;

    bltParams.Alpha = DXVA2_Fixed32OpaqueAlpha();

    hr = m_Processor->VideoProcessBlt(m_RenderTarget, &bltParams, &sample, 1, nullptr);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VideoProcessBlt() failed: %x",
                     hr);
    }

    m_Device->Present(nullptr, nullptr, nullptr, nullptr);
}
