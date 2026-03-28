// minwindef.h defines min() and max() macros that conflict with
// std::numeric_limits, which Qt uses in some of its headers.
#define NOMINMAX

#include <initguid.h>
#include "dxva2.h"
#include "dxutil.h"
#include "utils.h"
#include "../ffmpeg.h"
#include <streaming/streamutils.h>
#include <streaming/session.h>

extern "C" {
#include <libavutil/hwcontext_dxva2.h>
}

#include <SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dwmapi.h>

#include <Limelight.h>

using Microsoft::WRL::ComPtr;

typedef struct _VERTEX
{
    float x, y, z, rhw;
    float tu, tv;
} VERTEX, *PVERTEX;

DXVA2Renderer::DXVA2Renderer(int decoderSelectionPass) :
    IFFmpegRenderer(RendererType::DXVA2),
    m_DecoderSelectionPass(decoderSelectionPass),
    m_HwDeviceContext(nullptr),
    m_OverlayLock(0),
    m_FrameIndex(0),
    m_BlockingPresent(false),
    m_DeviceQuirks(0)
{
    // Use MMCSS scheduling for lower scheduling latency while we're streaming
    DwmEnableMMCSS(TRUE);
}

DXVA2Renderer::~DXVA2Renderer()
{
    DwmEnableMMCSS(FALSE);

    m_Device.Reset();
    m_RenderTarget.Reset();
    m_ProcService.Reset();
    m_Processor.Reset();

    for (auto& buffer : m_OverlayVertexBuffers) {
        buffer.Reset();
    }

    for (auto& texture : m_OverlayTextures) {
        texture.Reset();
    }

    av_buffer_unref(&m_HwDeviceContext);
    m_DeviceManager.Reset();
}

bool DXVA2Renderer::prepareDecoderContext(AVCodecContext* context, AVDictionary**)
{
    context->hw_device_ctx = av_buffer_ref(m_HwDeviceContext);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using DXVA2 accelerated renderer");

    return true;
}

bool DXVA2Renderer::initializeDecoder()
{
    HRESULT hr;

    if (isDecoderBlacklisted()) {
        return false;
    }

    UINT resetToken;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &m_DeviceManager);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2CreateDirect3DDeviceManager9() failed: %x",
                     hr);
        return false;
    }

    hr = m_DeviceManager->ResetDevice(m_Device.Get(), resetToken);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDirect3DDeviceManager9::ResetDevice() failed: %x",
                     hr);
        return false;
    }

    m_HwDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DXVA2);
    if (!m_HwDeviceContext) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to allocate DXVA2 device context");
        return false;
    }

    AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwDeviceContext->data;
    AVDXVA2DeviceContext* dxva2DeviceContext = (AVDXVA2DeviceContext*)deviceContext->hwctx;

    // FFmpeg assumes the lifetime of this object will be managed externally.
    // Unlike D3D11VA, it does not release a reference to this object.
    dxva2DeviceContext->devmgr = m_DeviceManager.Get();

    int err = av_hwdevice_ctx_init(m_HwDeviceContext);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to initialize D3D11VA device context: %d",
                     err);
        return false;
    }

    return true;
}

bool DXVA2Renderer::initializeRenderer()
{
    HRESULT hr = m_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_RenderTarget);
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

    m_Device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    m_Device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_Device->SetRenderState(D3DRS_LIGHTING, FALSE);

    m_Device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    m_Device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

    m_Device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_Device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);

    m_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    m_Device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    return true;
}

// This may be called multiple times during a stream
bool DXVA2Renderer::initializeVideoProcessor()
{
    // Check if we should use a video processor
    if (m_DeviceQuirks & DXVA2_QUIRK_NO_VP) {
        return true;
    }

    // Open a handle to our device that we share with FFmpeg
    HANDLE deviceHandle;
    HRESULT hr = m_DeviceManager->OpenDeviceHandle(&deviceHandle);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDirect3DDeviceManager9::OpenDeviceHandle() failed: %x",
                     hr);
        return false;
    }

    // Derive our video processor from the shared device
    hr = m_DeviceManager->GetVideoService(deviceHandle, IID_IDirectXVideoProcessorService, &m_ProcService);
    m_DeviceManager->CloseDeviceHandle(deviceHandle);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDirect3DDeviceManager9::GetVideoService() failed: %x",
                     hr);
        return false;
    }

    DXVA2_VideoProcessorCaps caps;
    D3DSURFACE_DESC renderTargetDesc;
    m_RenderTarget->GetDesc(&renderTargetDesc);
    hr = m_ProcService->GetVideoProcessorCaps(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, &caps);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetVideoProcessorCaps() failed for DXVA2_VideoProcProgressiveDevice: %x",
                     hr);
        return false;
    }

    if (!(caps.DeviceCaps & DXVA2_VPDev_HardwareDevice)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2_VideoProcProgressiveDevice is not hardware: %x",
                     caps.DeviceCaps);
        return false;
    }
    else if (!(caps.VideoProcessorOperations & DXVA2_VideoProcess_YUV2RGB) &&
             !(caps.VideoProcessorOperations & DXVA2_VideoProcess_YUV2RGBExtended)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2_VideoProcProgressiveDevice can't convert YUV2RGB: %x",
                     caps.VideoProcessorOperations);
        return false;
    }
    else if (!(caps.VideoProcessorOperations & DXVA2_VideoProcess_StretchX) ||
             !(caps.VideoProcessorOperations & DXVA2_VideoProcess_StretchY)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DXVA2_VideoProcProgressiveDevice can't stretch video: %x",
                     caps.VideoProcessorOperations);
        return false;
    }

    if (caps.DeviceCaps & DXVA2_VPDev_EmulatedDXVA1) {
        // DXVA2 over DXVA1 may have bad performance
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "DXVA2_VideoProcProgressiveDevice is DXVA1");
    }

    m_ProcService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Brightness, &m_BrightnessRange);
    m_ProcService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Contrast, &m_ContrastRange);
    m_ProcService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Hue, &m_HueRange);
    m_ProcService->GetProcAmpRange(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, DXVA2_ProcAmp_Saturation, &m_SaturationRange);

    hr = m_ProcService->CreateVideoProcessor(DXVA2_VideoProcProgressiveDevice, &m_Desc, renderTargetDesc.Format, 0, &m_Processor);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateVideoProcessor() failed for DXVA2_VideoProcProgressiveDevice: %x",
                     hr);
        return false;
    }

    return true;
}

bool DXVA2Renderer::initializeQuirksForAdapter(IDirect3D9Ex* d3d9ex, int adapterIndex)
{
    HRESULT hr;

    SDL_assert(m_DeviceQuirks == 0);
    SDL_assert(!m_Device);

    if (Utils::getEnvironmentVariableOverride("DXVA2_QUIRK_FLAGS", &m_DeviceQuirks)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using DXVA2 quirk override: 0x%x",
                    m_DeviceQuirks);
        return true;
    }

    UINT adapterCount = d3d9ex->GetAdapterCount();
    if (adapterCount > 1) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Detected multi-GPU system with %d GPUs",
                    adapterCount);
        m_DeviceQuirks |= DXVA2_QUIRK_MULTI_GPU;
    }

    D3DCAPS9 caps;
    hr = d3d9ex->GetDeviceCaps(adapterIndex, D3DDEVTYPE_HAL, &caps);
    if (SUCCEEDED(hr)) {
        D3DADAPTER_IDENTIFIER9 id;

        hr = d3d9ex->GetAdapterIdentifier(adapterIndex, 0, &id);
        if (SUCCEEDED(hr)) {
            if (id.VendorId == 0x8086 && !(m_VideoFormat & VIDEO_FORMAT_MASK_YUV444)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Avoiding IDirectXVideoProcessor API for YUV 4:2:0 on Intel GPU");

                // On Intel GPUs, we can get unwanted video "enhancements" due to post-processing
                // effects that the GPU driver forces on us. In many cases, this makes the video
                // actually look worse. We can avoid these by using StretchRect() instead on these
                // platforms.
                //
                // We don't do this for YUV 4:4:4 because Intel GPUs falsely claim to support AYUV
                // format conversion with StretchRect() via CheckDeviceFormatConversion() but the
                // output has completely incorrect colors. VideoProcessBlt() works fine though.
                m_DeviceQuirks |= DXVA2_QUIRK_NO_VP;
            }
            else if (id.VendorId == 0x4d4f4351) { // QCOM in ASCII
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Avoiding IDirectXVideoProcessor API on Qualcomm GPU");

                // On Qualcomm GPUs (all D3D9on12 GPUs?), the scaling quality of VideoProcessBlt()
                // is absolutely horrible. StretchRect() is much much better.
                m_DeviceQuirks |= DXVA2_QUIRK_NO_VP;
            }
            else if (id.VendorId == 0x1002 &&
                     (id.DriverVersion.HighPart > 0x1E0000 ||
                      (id.DriverVersion.HighPart == 0x1E0000 && HIWORD(id.DriverVersion.LowPart) >= 14000))) { // AMD 21.12.1 or later
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Using DestFormat quirk for recent AMD GPU driver");

                // AMD's GPU driver doesn't correctly handle color range conversion.
                //
                // This used to just work because we used Rec 709 Limited which happened to be what AMD's
                // driver defaulted to. However, AMD's driver behavior changed to default to Rec 709 Full
                // in the 21.12.1 driver, so we must adapt to that.
                //
                // 30.0.13037.1003 - 21.11.3 - Limited
                // 30.0.14011.3017 - 21.12.1 - Full
                //
                // To sort out this mess, we will use a quirk to tell us to populate DestFormat for AMD.
                // For other GPUs, we'll avoid populating it as was our previous behavior.
                m_DeviceQuirks |= DXVA2_QUIRK_SET_DEST_FORMAT;
            }
        }

        return true;
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetDeviceCaps() failed: %x", hr);
    }

    return false;
}

bool DXVA2Renderer::isDecoderBlacklisted()
{
    ComPtr<IDirect3D9> d3d9;
    HRESULT hr;
    bool result = false;

    if (qgetenv("DXVA2_DISABLE_DECODER_BLACKLIST") == "1") {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "DXVA2 decoder blacklist is disabled");
        return false;
    }

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
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "GPU driver: %s %d.%d.%d.%d",
                            id.Driver,
                            HIWORD(id.DriverVersion.HighPart),
                            LOWORD(id.DriverVersion.HighPart),
                            HIWORD(id.DriverVersion.LowPart),
                            LOWORD(id.DriverVersion.LowPart));

                if (DXUtil::isFormatHybridDecodedByHardware(m_VideoFormat, id.VendorId, id.DeviceId)) {
                    result = true;
                }
                // Intel drivers from before late-2017 had a bug that caused some strange artifacts
                // when decoding HEVC. Avoid HEVC on drivers prior to build 4836 which I confirmed
                // is not affected on my Intel HD 515. Also account for the driver version rollover
                // that happened with the 101.1069 series.
                // https://github.com/moonlight-stream/moonlight-qt/issues/32
                // https://www.intel.com/content/www/us/en/support/articles/000005654/graphics-drivers.html
                else if (id.VendorId == 0x8086 && HIWORD(id.DriverVersion.LowPart) < 100 && LOWORD(id.DriverVersion.LowPart) < 4836) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Detected buggy Intel GPU driver installed. Update your Intel GPU driver to enable HEVC!");
                    result = (m_VideoFormat & VIDEO_FORMAT_MASK_H265) != 0;
                }
            }
            else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "GetAdapterIdentifier() failed: %x", hr);
            }
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GetDeviceCaps() failed: %x", hr);
        }
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GetDirect3D() failed: %x", hr);
    }

    if (result) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPU decoding for format %x is blocked due to hardware limitations",
                    m_VideoFormat);
    }

    return result;
}

bool DXVA2Renderer::initializeDevice(SDL_Window* window, bool enableVsync)
{
    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    ComPtr<IDirect3D9Ex> d3d9ex;
    HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d9ex);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Direct3DCreate9Ex() failed: %x",
                     hr);
        return false;
    }

    int adapterIndex = SDL_Direct3D9GetAdapterIndex(SDL_GetWindowDisplayIndex(window));
    Uint32 windowFlags = SDL_GetWindowFlags(window);

    // Initialize quirks *before* calling CreateDeviceEx() to allow our below
    // logic to avoid a hang with NahimicOSD.dll's broken full-screen handling.
    if (!initializeQuirksForAdapter(d3d9ex.Get(), adapterIndex)) {
        return false;
    }

    D3DCAPS9 deviceCaps;
    d3d9ex->GetDeviceCaps(adapterIndex, D3DDEVTYPE_HAL, &deviceCaps);

    D3DDISPLAYMODEEX currentMode;
    currentMode.Size = sizeof(currentMode);
    d3d9ex->GetAdapterDisplayModeEx(adapterIndex, &currentMode, nullptr);

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.hDeviceWindow = info.info.win.window;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;

    if (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT) {
        // Verify 10-bit A2R10G10B10 color support. This is only available
        // as a display format in full-screen exclusive mode on DX9.
        hr = d3d9ex->CheckDeviceType(adapterIndex,
                                     D3DDEVTYPE_HAL,
                                     D3DFMT_A2R10G10B10,
                                     D3DFMT_A2R10G10B10,
                                     FALSE);
        if (FAILED(hr)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "GPU/driver doesn't support A2R10G10B10");
            return false;
        }
    }

    if ((windowFlags & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        d3dpp.Windowed = false;
        d3dpp.BackBufferWidth = currentMode.Width;
        d3dpp.BackBufferHeight = currentMode.Height;
        d3dpp.FullScreen_RefreshRateInHz = currentMode.RefreshRate;

        if (m_VideoFormat & VIDEO_FORMAT_MASK_10BIT) {
            d3dpp.BackBufferFormat = currentMode.Format = D3DFMT_A2R10G10B10;
        }
        else {
            d3dpp.BackBufferFormat = currentMode.Format;
        }
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
        d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;

        if (enableVsync) {
            // D3DSWAPEFFECT_FLIPEX requires at least 3 back buffers to allow us to
            // continue while DWM is waiting to render the surface to the display.
            // NVIDIA seems to be fine with 2, but AMD needs 3 to perform well.
            d3dpp.BackBufferCount = 3;
        }
        else {
            // With V-sync off, we need 1 more back buffer to render to while the
            // driver/DWM are holding the others.
            d3dpp.BackBufferCount = 4;
        }

        m_BlockingPresent = false;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Windowed mode with DWM running");
    }
    else if (enableVsync) {
        // Uncomposited desktop or full-screen exclusive mode with V-sync enabled
        // We will enable V-sync in this scenario to avoid tearing.
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 1;
        m_BlockingPresent = true;

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "V-Sync enabled");
    }
    else {
        // Uncomposited desktop or full-screen exclusive mode with V-sync disabled
        // We will allowing tearing for lowest latency.
        d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 1;
        m_BlockingPresent = false;

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
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateDeviceEx() failed: %x",
                     hr);
        return false;
    }

    // We must not call this for flip swapchains. It will counterintuitively
    // increase latency by forcing our Present() to block on DWM even when
    // using D3DPRESENT_INTERVAL_IMMEDIATE.
    if (d3dpp.SwapEffect != D3DSWAPEFFECT_FLIPEX) {
        hr = m_Device->SetMaximumFrameLatency(1);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SetMaximumFrameLatency() failed: %x",
                         hr);
            return false;
        }
    }

    return true;
}

bool DXVA2Renderer::initialize(PDECODER_PARAMETERS params)
{
    if (qgetenv("DXVA2_ENABLED") == "0") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "DXVA2 is disabled by environment variable");
        return false;
    }
    else if ((params->videoFormat & VIDEO_FORMAT_MASK_10BIT) && m_DecoderSelectionPass == 0) {
        // Avoid using DXVA2 for HDR10. While it can render 10-bit color, it doesn't support
        // the HDR colorspace and HDR display metadata required to enable HDR mode properly.
        return false;
    }
#ifndef Q_PROCESSOR_X86
    else if (qgetenv("DXVA2_ENABLED") != "1" && m_DecoderSelectionPass == 0) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "DXVA2 is disabled by default on ARM64. Set DXVA2_ENABLED=1 to override.");
        return false;
    }
#endif

    m_VideoFormat = params->videoFormat;

    if (!initializeDevice(params->window, params->enableVsync)) {
        return false;
    }

    if (!initializeDecoder()) {
        return false;
    }

    if (!initializeRenderer()) {
        return false;
    }

    return true;
}

void DXVA2Renderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    HRESULT hr;

    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    bool overlayEnabled = Session::get()->getOverlayManager().isOverlayEnabled(type);
    if (newSurface == nullptr && overlayEnabled) {
        // The overlay is enabled and there is no new surface. Leave the old texture alone.
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    ComPtr<IDirect3DTexture9> oldTexture = std::move(m_OverlayTextures[type]);
    ComPtr<IDirect3DVertexBuffer9> oldVertexBuffer = std::move(m_OverlayVertexBuffers[type]);
    SDL_AtomicUnlock(&m_OverlayLock);

    // If the overlay is disabled, we're done
    if (!overlayEnabled) {
        SDL_FreeSurface(newSurface);
        return;
    }

    // Create a dynamic texture to populate with our pixel data
    SDL_assert(!SDL_MUSTLOCK(newSurface));
    ComPtr<IDirect3DTexture9> newTexture;
    hr = m_Device->CreateTexture(newSurface->w,
                                 newSurface->h,
                                 1,
                                 D3DUSAGE_DYNAMIC,
                                 D3DFMT_A8R8G8B8,
                                 D3DPOOL_DEFAULT,
                                 &newTexture,
                                 nullptr);
    if (FAILED(hr)) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateTexture() failed: %x",
                     hr);
        return;
    }

    D3DLOCKED_RECT lockedRect;
    hr = newTexture->LockRect(0, &lockedRect, nullptr, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDirect3DTexture9::LockRect() failed: %x",
                     hr);
        return;
    }

    // Copy (and convert, if necessary) the surface pixels to the texture
    SDL_ConvertPixels(newSurface->w, newSurface->h, newSurface->format->format, newSurface->pixels,
                      newSurface->pitch, SDL_PIXELFORMAT_ARGB8888, lockedRect.pBits, lockedRect.Pitch);

    newTexture->UnlockRect(0);

    SDL_FRect renderRect = {};

    if (type == Overlay::OverlayStatusUpdate) {
        // Bottom Left
        renderRect.x = 0;
        renderRect.y = m_DisplayHeight - newSurface->h;
    }
    else if (type == Overlay::OverlayDebug) {
        // Top left
        renderRect.x = 0;
        renderRect.y = 0;
    }

    renderRect.w = newSurface->w;
    renderRect.h = newSurface->h;

    // The surface is no longer required
    SDL_FreeSurface(newSurface);
    newSurface = nullptr;

    // Compensate for D3D9's half pixel offset
    VERTEX verts[] =
    {
        {renderRect.x - 0.5f, renderRect.y - 0.5f, 0, 1, 0, 0},
        {renderRect.x - 0.5f, renderRect.y + renderRect.h - 0.5f, 0, 1, 0, 1},
        {renderRect.x + renderRect.w - 0.5f, renderRect.y + renderRect.h - 0.5f, 0, 1, 1, 1},
        {renderRect.x + renderRect.w - 0.5f, renderRect.y - 0.5f, 0, 1, 1, 0}
    };

    ComPtr<IDirect3DVertexBuffer9> newVertexBuffer;
    hr = m_Device->CreateVertexBuffer(sizeof(verts),
                                      D3DUSAGE_WRITEONLY,
                                      D3DFVF_XYZRHW | D3DFVF_TEX1,
                                      D3DPOOL_DEFAULT,
                                      &newVertexBuffer,
                                      nullptr);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateVertexBuffer() failed: %x",
                     hr);
        return;
    }

    PVOID targetVertexBuffer = nullptr;
    hr = newVertexBuffer->Lock(0, 0, &targetVertexBuffer, 0);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDirect3DVertexBuffer9::Lock() failed: %x",
                     hr);
        return;
    }

    RtlCopyMemory(targetVertexBuffer, verts, sizeof(verts));

    newVertexBuffer->Unlock();

    SDL_AtomicLock(&m_OverlayLock);
    m_OverlayVertexBuffers[type] = std::move(newVertexBuffer);
    m_OverlayTextures[type] = std::move(newTexture);
    SDL_AtomicUnlock(&m_OverlayLock);
}

void DXVA2Renderer::renderOverlay(Overlay::OverlayType type)
{
    HRESULT hr;

    if (!Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        return;
    }

    // If the overlay is being updated, just skip rendering it this frame
    if (!SDL_AtomicTryLock(&m_OverlayLock)) {
        return;
    }

    // Reference these objects so they don't immediately go away if the
    // overlay update thread tries to release them.
    ComPtr<IDirect3DTexture9> overlayTexture = m_OverlayTextures[type];
    ComPtr<IDirect3DVertexBuffer9> overlayVertexBuffer = m_OverlayVertexBuffers[type];
    SDL_AtomicUnlock(&m_OverlayLock);

    if (overlayTexture == nullptr) {
        return;
    }

    hr = m_Device->SetTexture(0, overlayTexture.Get());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SetTexture() failed: %x",
                     hr);
        return;
    }

    hr = m_Device->SetStreamSource(0, overlayVertexBuffer.Get(), 0, sizeof(VERTEX));
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SetStreamSource() failed: %x",
                     hr);
        return;
    }

    // Only enable blending when drawing the overlay
    m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    hr = m_Device->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
    m_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DrawPrimitive() failed: %x",
                     hr);
        return;
    }
}

int DXVA2Renderer::getDecoderColorspace()
{
    if (m_DeviceQuirks & DXVA2_QUIRK_NO_VP) {
        // StretchRect() assumes Rec 601 on Intel and Qualcomm GPUs.
        return COLORSPACE_REC_601;
    }
    else {
        // VideoProcessBlt() should properly handle whatever, since
        // we provide colorspace information. We used to use this because
        // we didn't know how to get AMD to respect the requested colorspace,
        // but now it's just because it's what we used before.
        return COLORSPACE_REC_709;
    }
}

int DXVA2Renderer::getDecoderCapabilities()
{
    return CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC |
           CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
}

void DXVA2Renderer::renderFrame(AVFrame *frame)
{
    IDirect3DSurface9* surface = reinterpret_cast<IDirect3DSurface9*>(frame->data[3]);
    HRESULT hr;

    if (hasFrameFormatChanged(frame)) {
        D3DSURFACE_DESC desc;
        surface->GetDesc(&desc);

        RtlZeroMemory(&m_Desc, sizeof(m_Desc));
        m_Desc.SampleWidth = frame->width;
        m_Desc.SampleHeight = frame->height;
        m_Desc.Format = desc.Format;

        m_Desc.SampleFormat.NominalRange = isFrameFullRange(frame) ? DXVA2_NominalRange_0_255 : DXVA2_NominalRange_16_235;
        m_Desc.SampleFormat.VideoLighting = DXVA2_VideoLighting_Unknown;
        m_Desc.SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;

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

        switch (getFrameColorspace(frame)) {
        case COLORSPACE_REC_709:
            m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
            break;
        case COLORSPACE_REC_601:
            m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
            break;
        default:
            m_Desc.SampleFormat.VideoTransferMatrix = DXVA2_VideoTransferMatrix_Unknown;
            break;
        }

        switch (frame->chroma_location) {
        case AVCHROMA_LOC_LEFT:
            m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Horizontally_Cosited |
                                                         DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes |
                                                         DXVA2_VideoChromaSubsampling_ProgressiveChroma;
            break;
        case AVCHROMA_LOC_CENTER:
            m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Vertically_AlignedChromaPlanes |
                                                         DXVA2_VideoChromaSubsampling_ProgressiveChroma;
            break;
        case AVCHROMA_LOC_TOPLEFT:
            m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Horizontally_Cosited |
                                                         DXVA2_VideoChromaSubsampling_Vertically_Cosited |
                                                         DXVA2_VideoChromaSubsampling_ProgressiveChroma;
            break;
        default:
            m_Desc.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_Unknown;
            break;
        }

        // Reinitialize the video processor for this new sample format
        if (!initializeVideoProcessor()) {
            m_Processor.Reset();
        }
    }

    DXVA2_VideoSample sample = {};
    sample.Start = m_FrameIndex;
    sample.End = m_FrameIndex + 1;
    sample.SrcSurface = surface;
    sample.SrcRect.right = frame->width;
    sample.SrcRect.bottom = frame->height;
    sample.SampleFormat = m_Desc.SampleFormat;
    sample.PlanarAlpha = DXVA2_Fixed32OpaqueAlpha();

    // Center in frame and preserve aspect ratio
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = frame->width;
    src.h = frame->height;
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

    if (m_DeviceQuirks & DXVA2_QUIRK_SET_DEST_FORMAT) {
        bltParams.DestFormat = m_Desc.SampleFormat;
    }
    else {
        bltParams.DestFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    }

    bltParams.ProcAmpValues.Brightness = m_BrightnessRange.DefaultValue;
    bltParams.ProcAmpValues.Contrast = m_ContrastRange.DefaultValue;
    bltParams.ProcAmpValues.Hue = m_HueRange.DefaultValue;
    bltParams.ProcAmpValues.Saturation = m_SaturationRange.DefaultValue;

    bltParams.Alpha = DXVA2_Fixed32OpaqueAlpha();

    hr = m_Device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 0.0f, 0);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Clear() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }

    hr = m_Device->BeginScene();
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BeginScene() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }

    if (m_Processor) {
        hr = m_Processor->VideoProcessBlt(m_RenderTarget.Get(), &bltParams, &sample, 1, nullptr);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "VideoProcessBlt() failed, falling back to StretchRect(): %x",
                         hr);
            m_Processor.Reset();
        }
    }

    if (!m_Processor) {
        // StretchRect() assumes Rec 601 on Intel and Qualcomm GPUs.
        SDL_assert(m_Desc.SampleFormat.VideoTransferMatrix == DXVA2_VideoTransferMatrix_BT601);

        // This function doesn't trigger any of Intel's garbage video "enhancements"
        hr = m_Device->StretchRect(surface, &sample.SrcRect, m_RenderTarget.Get(), &sample.DstRect, D3DTEXF_LINEAR);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "StretchRect() failed: %x",
                         hr);
            SDL_Event event;
            event.type = SDL_RENDER_DEVICE_RESET;
            SDL_PushEvent(&event);
            return;
        }
    }

    // Render overlays on top of the video stream
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        renderOverlay((Overlay::OverlayType)i);
    }

    hr = m_Device->EndScene();
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "EndScene() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }

    do {
        // Use D3DPRESENT_DONOTWAIT if present may block in order to avoid holding the giant
        // lock around this D3D device for excessive lengths of time (blocking concurrent decoding tasks).
        hr = m_Device->PresentEx(nullptr, nullptr, nullptr, nullptr, m_BlockingPresent ? D3DPRESENT_DONOTWAIT : 0);
        if (hr == D3DERR_WASSTILLDRAWING) {
            SDL_Delay(1);
        }
    } while (hr == D3DERR_WASSTILLDRAWING);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "PresentEx() failed: %x",
                     hr);
        SDL_Event event;
        event.type = SDL_RENDER_DEVICE_RESET;
        SDL_PushEvent(&event);
        return;
    }
}
