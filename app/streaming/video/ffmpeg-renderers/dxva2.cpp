#include <initguid.h>
#include "dxva2.h"
#include "../ffmpeg.h"
#include <streaming/streamutils.h>

#include <SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <VersionHelpers.h>
#include <dwmapi.h>

#include <Limelight.h>

DEFINE_GUID(DXVADDI_Intel_ModeH264_E, 0x604F8E68,0x4951,0x4C54,0x88,0xFE,0xAB,0xD2,0x5C,0x15,0xB3,0xD6);

#define SAFE_COM_RELEASE(x) if (x) { (x)->Release(); }

DXVA2Renderer::DXVA2Renderer() :
    m_DecService(nullptr),
    m_Decoder(nullptr),
    m_SurfacesUsed(0),
    m_Pool(nullptr),
    m_Device(nullptr),
    m_RenderTarget(nullptr),
    m_ProcService(nullptr),
    m_Processor(nullptr),
    m_FrameIndex(0)
{
    RtlZeroMemory(m_DecSurfaces, sizeof(m_DecSurfaces));
    RtlZeroMemory(&m_DXVAContext, sizeof(m_DXVAContext));

    // Use MMCSS scheduling for lower scheduling latency while we're streaming
    DwmEnableMMCSS(TRUE);
}

DXVA2Renderer::~DXVA2Renderer()
{
    DwmEnableMMCSS(FALSE);

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
}

void DXVA2Renderer::ffPoolDummyDelete(void*, uint8_t*)
{
    /* Do nothing */
}

AVBufferRef* DXVA2Renderer::ffPoolAlloc(void* opaque, int)
{
    DXVA2Renderer* me = reinterpret_cast<DXVA2Renderer*>(opaque);

    if (me->m_SurfacesUsed < ARRAYSIZE(me->m_DecSurfaces)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "DXVA2 decoder surface high-water mark: %d",
                    me->m_SurfacesUsed);
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

    context->get_buffer2 = ffGetBuffer2;

    m_Pool = av_buffer_pool_init2(ARRAYSIZE(m_DecSurfaces), this, ffPoolAlloc, nullptr);
    if (!m_Pool) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed create buffer pool");
        return false;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DXVA2 accelerated renderer");

    return true;
}

int DXVA2Renderer::ffGetBuffer2(AVCodecContext* context, AVFrame* frame, int)
{
    DXVA2Renderer* me = (DXVA2Renderer*)((FFmpegVideoDecoder*)context->opaque)->getRenderer();

    frame->buf[0] = av_buffer_pool_get(me->m_Pool);
    if (!frame->buf[0]) {
        return AVERROR(ENOMEM);
    }

    frame->data[3] = frame->buf[0]->data;
    frame->format = AV_PIX_FMT_DXVA2_VLD;
    frame->width = me->m_VideoWidth;
    frame->height = me->m_VideoHeight;

    return 0;
}

bool DXVA2Renderer::initializeDecoder()
{
    HRESULT hr;

    if (isDecoderBlacklisted()) {
        return false;
    }

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

    // Alignment was already taken care of
    SDL_assert(m_Desc.SampleWidth % 16 == 0);
    SDL_assert(m_Desc.SampleHeight % 16 == 0);
    hr = m_DecService->CreateSurface(m_Desc.SampleWidth,
                                     m_Desc.SampleHeight,
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

    m_DisplayWidth = renderTargetDesc.Width;
    m_DisplayHeight = renderTargetDesc.Height;

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
                            caps.VideoProcessorOperations);
                continue;
            }
            else if (!(caps.VideoProcessorOperations & DXVA2_VideoProcess_StretchX) ||
                     !(caps.VideoProcessorOperations & DXVA2_VideoProcess_StretchY)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Device %d can't stretch video: %x",
                            i,
                            caps.VideoProcessorOperations);
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

bool DXVA2Renderer::isDecoderBlacklisted()
{
    IDirect3D9* d3d9;
    HRESULT hr;
    bool result = false;

    // TODO: Update for HEVC Main10
    SDL_assert(m_VideoFormat != VIDEO_FORMAT_H265_MAIN10);

    hr = m_Device->GetDirect3D(&d3d9);
    if (SUCCEEDED(hr)) {
        D3DCAPS9 caps;

        hr = m_Device->GetDeviceCaps(&caps);
        if (SUCCEEDED(hr)) {
            D3DADAPTER_IDENTIFIER9 id;

            hr = d3d9->GetAdapterIdentifier(caps.AdapterOrdinal, 0, &id);
            if (SUCCEEDED(hr)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Detected GPU: %s (%x:%x)",
                            id.Description,
                            id.VendorId,
                            id.DeviceId);

                if (id.VendorId == 0x8086) {
                    // Intel seems to encode the series in the high byte of
                    // the device ID. We want to avoid the "Partial" acceleration
                    // support explicitly. Those will claim to have HW acceleration
                    // but perform badly.
                    // https://en.wikipedia.org/wiki/Intel_Graphics_Technology#Capabilities_(GPU_video_acceleration)
                    // https://raw.githubusercontent.com/GameTechDev/gpudetect/master/IntelGfx.cfg
                    switch (id.DeviceId & 0xFF00) {
                    case 0x0400: // Haswell
                    case 0x0A00: // Haswell
                    case 0x0D00: // Haswell
                    case 0x1600: // Broadwell
                    case 0x2200: // Cherry Trail and Braswell
                        // Blacklist these for HEVC to avoid hybrid decode
                        result = (m_VideoFormat & VIDEO_FORMAT_MASK_H265) != 0;
                        break;
                    default:
                        // Everything else is fine with whatever it says it supports
                        result = false;
                        break;
                    }
                }
                else if (id.VendorId == 0x10DE) {
                    // For NVIDIA, we wait to avoid those GPUs with Feature Set E
                    // for HEVC decoding, since that's hybrid.
                    // https://en.wikipedia.org/wiki/Nvidia_PureVideo
                    // http://envytools.readthedocs.io/en/latest/hw/pciid.html (missing GM200)
                    if ((id.DeviceId >= 0x1340 && id.DeviceId <= 0x137F) || // GM108
                            (id.DeviceId >= 0x1380 && id.DeviceId <= 0x13BF) || // GM107
                            (id.DeviceId >= 0x13C0 && id.DeviceId <= 0x13FF) || // GM204
                            (id.DeviceId >= 0x1617 && id.DeviceId <= 0x161A) || // GM204
                            (id.DeviceId == 0x1667) || // GM204
                            (id.DeviceId >= 0x17C0 && id.DeviceId <= 0x17FF)) { // GM200
                        // Avoid HEVC on Feature Set E GPUs
                        result = (m_VideoFormat & VIDEO_FORMAT_MASK_H265) != 0;
                    }
                    else {
                        result = false;
                    }
                }
                else if (id.VendorId == 0x1002) {
                    // AMD doesn't seem to do hybrid acceleration?
                    result = false;
                }
                else {
                    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                "Unrecognized vendor ID: %x",
                                id.VendorId);
                }
            }
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GetDeviceCaps() failed: %x", hr);
        }

        d3d9->Release();
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetDirect3D() failed: %x", hr);
    }

    if (result) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPU blacklisted for format %x",
                    m_VideoFormat);
    }

    return result;
}

bool DXVA2Renderer::initializeDevice(SDL_Window* window, bool enableVsync)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    IDirect3D9Ex* d3d9ex;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Direct3DCreate9Ex() failed: %x",
                     hr);
        return false;
    }

    int adapterIndex = SDL_Direct3D9GetAdapterIndex(SDL_GetWindowDisplayIndex(window));
    Uint32 windowFlags = SDL_GetWindowFlags(window);

    D3DCAPS9 deviceCaps;
    d3d9ex->GetDeviceCaps(adapterIndex, D3DDEVTYPE_HAL, &deviceCaps);

    D3DDISPLAYMODEEX currentMode;
    currentMode.Size = sizeof(currentMode);
    d3d9ex->GetAdapterDisplayModeEx(adapterIndex, &currentMode, nullptr);

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.hDeviceWindow = info.info.win.window;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    if ((windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        d3dpp.Windowed = false;
        d3dpp.BackBufferWidth = currentMode.Width;
        d3dpp.BackBufferHeight = currentMode.Height;
        d3dpp.FullScreen_RefreshRateInHz = currentMode.RefreshRate;
        d3dpp.BackBufferFormat = currentMode.Format;
    }
    else {
        d3dpp.Windowed = true;
        d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

        SDL_GetWindowSize(window, (int*)&d3dpp.BackBufferWidth, (int*)&d3dpp.BackBufferHeight);
    }

    BOOL dwmEnabled;
    DwmIsCompositionEnabled(&dwmEnabled);
    if (d3dpp.Windowed && dwmEnabled) {
        // If composition enabled, disable v-sync and let DWM manage things
        // to reduce latency by avoiding double v-syncing.
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        // D3DSWAPEFFECT_FLIPEX requires at least 2 back buffers to allow us to
        // continue while DWM is waiting to render the surface to the display.
        d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
        d3dpp.BackBufferCount = 2;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Windowed mode with DWM running");
    }
    else if (enableVsync) {
        // Uncomposited desktop or full-screen exclusive mode with V-sync enabled
        // We will enable V-sync in this scenario to avoid tearing.
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        d3dpp.BackBufferCount = 1;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "V-Sync enabled");
    }
    else {
        // Uncomposited desktop or full-screen exclusive mode with V-sync disabled
        // We will allowing tearing for lowest latency.
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        d3dpp.BackBufferCount = 1;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "V-Sync disabled in tearing mode");
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Windowed: %d | Present Interval: %x",
                d3dpp.Windowed, d3dpp.PresentationInterval);

    // FFmpeg requires this attribute for doing asynchronous decoding
    // in a separate thread with this device.
    int deviceFlags = D3DCREATE_MULTITHREADED;

    if (deviceCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) {
        deviceFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "No hardware vertex processing support!");
        deviceFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    hr = d3d9ex->CreateDeviceEx(adapterIndex,
                                D3DDEVTYPE_HAL,
                                d3dpp.hDeviceWindow,
                                deviceFlags,
                                &d3dpp,
                                d3dpp.Windowed ? nullptr : &currentMode,
                                &m_Device);

    d3d9ex->Release();

    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateDeviceEx() failed: %x",
                     hr);
        return false;
    }

    hr = m_Device->SetMaximumFrameLatency(1);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SetMaximumFrameLatency() failed: %x",
                     hr);
        return false;
    }

    return true;
}

bool DXVA2Renderer::initialize(SDL_Window* window, int videoFormat, int width, int height, int, bool enableVsync)
{
    m_VideoFormat = videoFormat;
    m_VideoWidth = width;
    m_VideoHeight = height;

    RtlZeroMemory(&m_Desc, sizeof(m_Desc));

    int alignment;

    // HEVC using DXVA requires 128B alignment
    if (m_VideoFormat & VIDEO_FORMAT_MASK_H265) {
        alignment = 128;
    }
    else {
        alignment = 16;
    }

    m_Desc.SampleWidth = FFALIGN(m_VideoWidth, alignment);
    m_Desc.SampleHeight = FFALIGN(m_VideoHeight, alignment);
    m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
    m_Desc.SampleFormat.NominalRange = DXVA2_NominalRange_Unknown;
    m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
    m_Desc.SampleFormat.VideoLighting = DXVA2_VideoLighting_Unknown;
    m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_Unknown;
    m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_Unknown;
    m_Desc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    m_Desc.Format = (D3DFORMAT)MAKEFOURCC('N','V','1','2');

    if (!initializeDevice(window, enableVsync)) {
        return false;
    }

    if (!initializeDecoder()) {
        return false;
    }

    if (!initializeRenderer()) {
        return false;
    }

    // For some reason, using Direct3D9Ex breaks this with multi-monitor setups.
    // When focus is lost, the window is minimized then immediately restored without
    // input focus. This glitches out the renderer and a bunch of other stuff.
    // Direct3D9Ex itself seems to have this minimize on focus loss behavior on its
    // own, so just disable SDL's handling of the focus loss event.
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    return true;
}

bool DXVA2Renderer::needsTestFrame()
{
    // We validate the DXVA2 profiles are supported
    // in initialize() so no test frame is required
    return false;
}

void DXVA2Renderer::renderFrameAtVsync(AVFrame *frame)
{
    IDirect3DSurface9* surface = reinterpret_cast<IDirect3DSurface9*>(frame->data[3]);
    HRESULT hr;

    switch (frame->color_range) {
    case AVCOL_RANGE_JPEG:
        m_Desc.SampleFormat.NominalRange = DXVA2_NominalRange_0_255;
        break;
    case AVCOL_RANGE_MPEG:
        m_Desc.SampleFormat.NominalRange = DXVA2_NominalRange_16_235;
        break;
    default:
        m_Desc.SampleFormat.NominalRange = DXVA2_NominalRange_Unknown;
        break;
    }

    switch (frame->color_primaries) {
    case AVCOL_PRI_BT709:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
        break;
    case AVCOL_PRI_BT470M:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysM;
        break;
    case AVCOL_PRI_BT470BG:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysBG;
        break;
    case AVCOL_PRI_SMPTE170M:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE170M;
        break;
    case AVCOL_PRI_SMPTE240M:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE240M;
        break;
    default:
        m_Desc.SampleFormat.VideoPrimaries = DXVA2_VideoPrimaries_Unknown;
        break;
    }

    switch (frame->color_trc) {
    case AVCOL_TRC_SMPTE170M:
    case AVCOL_TRC_BT709:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_709;
        break;
    case AVCOL_TRC_LINEAR:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_10;
        break;
    case AVCOL_TRC_GAMMA22:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_22;
        break;
    case AVCOL_TRC_GAMMA28:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_28;
        break;
    case AVCOL_TRC_SMPTE240M:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_240M;
        break;
    case AVCOL_TRC_IEC61966_2_1:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_sRGB;
        break;
    default:
        m_Desc.SampleFormat.VideoTransferFunction = DXVA2_VideoTransFunc_Unknown;
        break;
    }

    switch (frame->colorspace) {
    case AVCOL_SPC_BT709:
        m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
        break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
        break;
    case AVCOL_SPC_SMPTE240M:
        m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_SMPTE240M;
        break;
    default:
        m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
        break;
    }

    switch (frame->chroma_location) {
    case AVCHROMA_LOC_LEFT:
        m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Horizontally_Cosited |
                                                     DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes;
        break;
    case AVCHROMA_LOC_CENTER:
        m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes;
        break;
    case AVCHROMA_LOC_TOPLEFT:
        m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Horizontally_Cosited |
                                                     DXVA2_VideoChromaSubsampling_Vertically_Cosited;
        break;
    default:
        m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
        break;
    }

    DXVA2_VideoSample sample = {};
    sample.Start = m_FrameIndex;
    sample.End = m_FrameIndex + 1;
    sample.SrcSurface = surface;
    sample.SrcRect.right = m_VideoWidth;
    sample.SrcRect.bottom = m_VideoHeight;
    sample.SampleFormat = m_Desc.SampleFormat;
    sample.PlanarAlpha = DXVA2_Fixed32OpaqueAlpha();

    // Center in frame and preserve aspect ratio
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_VideoWidth;
    src.h = m_VideoHeight;
    dst.x = dst.y = 0;
    dst.w = m_DisplayWidth;
    dst.h = m_DisplayHeight;

    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);

    sample.DstRect.left = dst.x;
    sample.DstRect.right = dst.x + dst.w;
    sample.DstRect.top = dst.y;
    sample.DstRect.bottom = dst.y + dst.h;

    DXVA2_VideoProcessBltParams bltParams = {};

    bltParams.TargetFrame = m_FrameIndex++;
    bltParams.TargetRect = sample.DstRect;
    bltParams.BackgroundColor.Alpha = 0xFFFF;

    bltParams.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

    bltParams.ProcAmpValues.Brightness = m_BrightnessRange.DefaultValue;
    bltParams.ProcAmpValues.Contrast = m_ContrastRange.DefaultValue;
    bltParams.ProcAmpValues.Hue = m_HueRange.DefaultValue;
    bltParams.ProcAmpValues.Saturation = m_SaturationRange.DefaultValue;

    bltParams.Alpha = DXVA2_Fixed32OpaqueAlpha();

    m_Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0.0f, 0);

    hr = m_Processor->VideoProcessBlt(m_RenderTarget, &bltParams, &sample, 1, nullptr);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "VideoProcessBlt() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_TARGETS_RESET;
        SDL_PushEvent(&event);
        return;
    }

    hr = m_Device->PresentEx(nullptr, nullptr, nullptr, nullptr, 0);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PresentEx() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_TARGETS_RESET;
        SDL_PushEvent(&event);
        return;
    }
}
