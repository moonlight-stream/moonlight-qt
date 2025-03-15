// For D3D11_DECODER_PROFILE values
#include <initguid.h>

#include "d3d11va.h"
#include "dxutil.h"
#include "path.h"

#include "streaming/streamutils.h"
#include "streaming/session.h"
#include "streaming/video/videoenhancement.h"

#include "public/common/AMFFactory.h"
#include "public/include/core/Platform.h"
// Video upscaling & Sharpening
#include "public/include/components/HQScaler.h"

#include <cmath>
#include <Limelight.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

extern "C" {
#include <libavutil/mastering_display_metadata.h>
}

#include <SDL_syswm.h>
#include <VersionHelpers.h>

#include <dwmapi.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// Standard DXVA GUIDs for HEVC RExt profiles (redefined for compatibility with pre-24H2 SDKs)
DEFINE_GUID(k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN_444,   0x4008018f, 0xf537, 0x4b36, 0x98, 0xcf, 0x61, 0xaf, 0x8a, 0x2c, 0x1a, 0x33);
DEFINE_GUID(k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10_444, 0x0dabeffa, 0x4458, 0x4602, 0xbc, 0x03, 0x07, 0x95, 0x65, 0x9d, 0x61, 0x7c);

typedef struct _VERTEX
{
    float x, y;
    float tu, tv;
} VERTEX, *PVERTEX;

#define CSC_MATRIX_RAW_ELEMENT_COUNT 9
#define CSC_MATRIX_PACKED_ELEMENT_COUNT 12

static const float k_CscMatrix_Bt601Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.1644f, 1.1644f, 1.1644f,
    0.0f, -0.3917f, 2.0172f,
    1.5960f, -0.8129f, 0.0f,
};
static const float k_CscMatrix_Bt601Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.0f, 1.0f, 1.0f,
    0.0f, -0.3441f, 1.7720f,
    1.4020f, -0.7141f, 0.0f,
};
static const float k_CscMatrix_Bt709Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.1644f, 1.1644f, 1.1644f,
    0.0f, -0.2132f, 2.1124f,
    1.7927f, -0.5329f, 0.0f,
};
static const float k_CscMatrix_Bt709Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.0f, 1.0f, 1.0f,
    0.0f, -0.1873f, 1.8556f,
    1.5748f, -0.4681f, 0.0f,
};
static const float k_CscMatrix_Bt2020Lim[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.1644f, 1.1644f, 1.1644f,
    0.0f, -0.1874f, 2.1418f,
    1.6781f, -0.6505f, 0.0f,
};
static const float k_CscMatrix_Bt2020Full[CSC_MATRIX_RAW_ELEMENT_COUNT] = {
    1.0f, 1.0f, 1.0f,
    0.0f, -0.1646f, 1.8814f,
    1.4746f, -0.5714f, 0.0f,
};

#define OFFSETS_ELEMENT_COUNT 3

static const float k_Offsets_Lim[OFFSETS_ELEMENT_COUNT] = { 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f };
static const float k_Offsets_Full[OFFSETS_ELEMENT_COUNT] = { 0.0f, 128.0f / 255.0f, 128.0f / 255.0f };

typedef struct _CSC_CONST_BUF
{
    // CscMatrix value from above but packed appropriately
    float cscMatrix[CSC_MATRIX_PACKED_ELEMENT_COUNT];

    // YUV offset values from above
    float offsets[OFFSETS_ELEMENT_COUNT];

    // Padding float to be a multiple of 16 bytes
    float padding;
} CSC_CONST_BUF, *PCSC_CONST_BUF;
static_assert(sizeof(CSC_CONST_BUF) % 16 == 0, "Constant buffer sizes must be a multiple of 16");

static const std::array<const char*, D3D11VARenderer::PixelShaders::_COUNT> k_VideoShaderNames =
{
    "d3d11_genyuv_pixel.fxc",
    "d3d11_bt601lim_pixel.fxc",
    "d3d11_bt2020lim_pixel.fxc",
    "d3d11_ayuv_pixel.fxc",
    "d3d11_y410_pixel.fxc",
};

D3D11VARenderer::D3D11VARenderer(int decoderSelectionPass)
    : IFFmpegRenderer(RendererType::D3D11VA),
      m_DecoderSelectionPass(decoderSelectionPass),
      m_DevicesWithCodecSupport(0),
      m_DevicesWithFL11Support(0),
      m_LastColorSpace(-1),
      m_LastFullRange(false),
      m_LastColorTrc(AVCOL_TRC_UNSPECIFIED),
      m_AllowTearing(false),
      m_OverlayLock(0),
      m_HwDeviceContext(nullptr),
      m_HwFramesContext(nullptr),
      m_AmfContext(nullptr),
      m_AmfSurface(nullptr),
      m_AmfData(nullptr),
      m_AmfUpScaler(nullptr),
      m_AmfInitialized(false)
{
    // RtlZeroMemory(m_VideoTextureResourceViews, sizeof(m_VideoTextureResourceViews));

    m_ContextLock = SDL_CreateMutex();

    DwmEnableMMCSS(TRUE);

    m_VideoEnhancement = &VideoEnhancement::getInstance();
}

D3D11VARenderer::~D3D11VARenderer()
{
    DwmEnableMMCSS(FALSE);

    SDL_DestroyMutex(m_ContextLock);

    m_VideoVertexBuffer.Reset();
    for (auto& shader : m_VideoPixelShaders) {
        shader.Reset();
    }

    for (auto& textureSrvs : m_VideoTextureResourceViews) {
        for (auto& srv : textureSrvs) {
            srv.Reset();
        }
    }

    m_VideoTexture.Reset();

    for (auto& buffer : m_OverlayVertexBuffers) {
        buffer.Reset();
    }

    for (auto& srv : m_OverlayTextureResourceViews) {
        srv.Reset();
    }

    for (auto& texture : m_OverlayTextures) {
        texture.Reset();
    }

    m_OverlayPixelShader.Reset();

    m_RenderTargetView.Reset();
    m_SwapChain.Reset();

    // cleanup AMF instances
    if(m_AmfUpScaler){
        // Up Scaler
        m_AmfUpScaler->Terminate();
        m_AmfUpScaler = nullptr;
    }
    if(m_AmfContext){
        // Context
        m_AmfContext->Terminate();
        m_AmfContext = nullptr;
    }

    g_AMFFactory.Terminate();

    if(m_VideoProcessorEnumerator){
        m_VideoProcessorEnumerator.Reset();
    }
    if(m_VideoProcessor){
        m_VideoProcessor.Reset();
    }

#ifdef QT_DEBUG
    ComPtr<ID3D11Debug> debugDevice;
    if(FAILED(m_Device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(debugDevice.GetAddressOf())))) {
        debugDevice = nullptr;
    }
#endif

    av_buffer_unref(&m_HwFramesContext);
    av_buffer_unref(&m_HwDeviceContext);

    // Force destruction of the swapchain immediately
    if (m_DeviceContext != nullptr) {
        m_DeviceContext->ClearState();
        m_DeviceContext->Flush();
    }

// Uncomment the lines in the QT_DEBUG section if you need to debug DirectX objects
#ifdef QT_DEBUG
    // if(debugDevice) {
    //     debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_IGNORE_INTERNAL);
    // }
    // CComPtr<IDXGIDebug1> pDebugDevice;
    // if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebugDevice))))
    // {
    //     pDebugDevice->ReportLiveObjects(DXGI_DEBUG_DX, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    // }
#endif

    m_Device.Reset();
    m_DeviceContext.Reset();
    m_Factory.Reset();
}

/**
 * \brief Set HDR MetaData information for Stream and Output
 *
 * Get the HDR MetaData via LimeLight library sent by Sunshine to apply to the Stream.
 * Get the monitor HDR MetaData where the application is running to apply to the Output.
 *
 * \param bool enabled At true it enables the HDR settings
 * \return void
 */
void D3D11VARenderer::setHdrMode(bool enabled){

    // m_VideoProcessor needs to be available to be set,
    // and it makes sense only when HDR is enabled from the UI
    if(!enabled || !m_VideoProcessor || !(m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT))
        return;

    DXGI_HDR_METADATA_HDR10 streamHDRMetaData;
    DXGI_HDR_METADATA_HDR10 outputHDRMetaData;

    // Prepare HDR Meta Data for Streamed content
    bool streamSet = false;
    SS_HDR_METADATA hdrMetadata;
    if (LiGetHdrMetadata(&hdrMetadata)) {
        streamHDRMetaData.RedPrimary[0] = hdrMetadata.displayPrimaries[0].x;
        streamHDRMetaData.RedPrimary[1] = hdrMetadata.displayPrimaries[0].y;
        streamHDRMetaData.GreenPrimary[0] = hdrMetadata.displayPrimaries[1].x;
        streamHDRMetaData.GreenPrimary[1] = hdrMetadata.displayPrimaries[1].y;
        streamHDRMetaData.BluePrimary[0] = hdrMetadata.displayPrimaries[2].x;
        streamHDRMetaData.BluePrimary[1] = hdrMetadata.displayPrimaries[2].y;
        streamHDRMetaData.WhitePoint[0] = hdrMetadata.whitePoint.x;
        streamHDRMetaData.WhitePoint[1] = hdrMetadata.whitePoint.y;
        streamHDRMetaData.MaxMasteringLuminance = hdrMetadata.maxDisplayLuminance;
        streamHDRMetaData.MinMasteringLuminance = hdrMetadata.minDisplayLuminance;

        // As the Content is unknown since it is streamed, MaxCLL and MaxFALL cannot be evaluated from the source on the fly,
        // therefore streamed source returns 0 as value for both. We can safetly set them to 0.
        streamHDRMetaData.MaxContentLightLevel = 0;
        streamHDRMetaData.MaxFrameAverageLightLevel = 0;

        // Set HDR Stream (input) Meta data
        m_VideoContext->VideoProcessorSetStreamHDRMetaData(
            m_VideoProcessor.Get(),
            0,
            DXGI_HDR_METADATA_TYPE_HDR10,
            sizeof(DXGI_HDR_METADATA_HDR10),
            &streamHDRMetaData
            );

        streamSet = true;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Set stream HDR mode: %s", streamSet ? "enabled" : "disabled");

    // Prepare HDR Meta Data to match the monitor HDR specifications
    // Retreive the monitor HDR metadata where the application is displayed
    int appAdapterIndex = 0;
    int appOutputIndex = 0;
    bool displaySet = false;
    if (SDL_DXGIGetOutputInfo(SDL_GetWindowDisplayIndex(m_DecoderParams.window), &appAdapterIndex, &appOutputIndex)){
        ComPtr<IDXGIAdapter1> adapter;
        ComPtr<IDXGIOutput> output;
        UINT outputIndex = appOutputIndex;
        if(SUCCEEDED(m_Factory->EnumAdapters1(appAdapterIndex, &adapter))){
            if(SUCCEEDED(adapter->EnumOutputs(outputIndex, &output))){
                ComPtr<IDXGIOutput6> output6;
                if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput6), (void**)&output6))) {
                    DXGI_OUTPUT_DESC1 desc1;
                    if (output6) {
                        output6->GetDesc1(&desc1);
                        // Magic constants to convert to fixed point.
                        // https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_5/ns-dxgi1_5-dxgi_hdr_metadata_hdr10
                        static constexpr int kPrimariesFixedPoint = 50000;
                        static constexpr int kMinLuminanceFixedPoint = 10000;

                        // Format Monitor HDR MetaData
                        outputHDRMetaData.RedPrimary[0] = desc1.RedPrimary[0] * kPrimariesFixedPoint;
                        outputHDRMetaData.RedPrimary[1] = desc1.RedPrimary[1] * kPrimariesFixedPoint;
                        outputHDRMetaData.GreenPrimary[0] = desc1.GreenPrimary[0] * kPrimariesFixedPoint;
                        outputHDRMetaData.GreenPrimary[1] = desc1.GreenPrimary[1] * kPrimariesFixedPoint;
                        outputHDRMetaData.BluePrimary[0] = desc1.BluePrimary[0] * kPrimariesFixedPoint;
                        outputHDRMetaData.BluePrimary[1] = desc1.BluePrimary[1] * kPrimariesFixedPoint;
                        outputHDRMetaData.WhitePoint[0] = desc1.WhitePoint[0] * kPrimariesFixedPoint;
                        outputHDRMetaData.WhitePoint[1] = desc1.WhitePoint[1] * kPrimariesFixedPoint;
                        outputHDRMetaData.MaxMasteringLuminance = desc1.MaxLuminance;
                        outputHDRMetaData.MinMasteringLuminance = desc1.MinLuminance * kMinLuminanceFixedPoint;
                        // Set it the same as streamed source which is 0 by default as it cannot be evaluated on the fly.
                        outputHDRMetaData.MaxContentLightLevel = 0;
                        outputHDRMetaData.MaxFrameAverageLightLevel = 0;

                        // Prepare HDR for the OutPut Monitor
                        m_VideoContext->VideoProcessorSetOutputHDRMetaData(
                            m_VideoProcessor.Get(),
                            DXGI_HDR_METADATA_TYPE_HDR10,
                            sizeof(DXGI_HDR_METADATA_HDR10),
                            &outputHDRMetaData
                            );

                        displaySet = true;
                    }
                }
            }
        }
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Set display HDR mode: %s", displaySet ? "enabled" : "disabled");

}

bool D3D11VARenderer::createDeviceByAdapterIndex(int adapterIndex, bool* adapterNotFound)
{
    const D3D_FEATURE_LEVEL supportedFeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    bool success = false;
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapterDesc;
    ComPtr<ID3D11Multithread> pMultithread;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr;

    SDL_assert(!m_Device);
    SDL_assert(!m_DeviceContext);

    hr = m_Factory->EnumAdapters1(adapterIndex, adapter.GetAddressOf());
    if (hr == DXGI_ERROR_NOT_FOUND) {
        // Expected at the end of enumeration
        goto Exit;
    }
    else if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIFactory::EnumAdapters1() failed: %x",
                     hr);
        goto Exit;
    }

    hr = adapter->GetDesc1(&adapterDesc);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIAdapter::GetDesc() failed: %x",
                     hr);
        goto Exit;
    }

    if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        // Skip the WARP device. We know it will fail.
        goto Exit;
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Detected GPU %d: %S (%x:%x)",
                adapterIndex,
                adapterDesc.Description,
                adapterDesc.VendorId,
                adapterDesc.DeviceId);

    // D3D11_CREATE_DEVICE_DEBUG generates more information about DirectX11 objects for debugging.
    // https://seanmiddleditch.github.io/direct3d-11-debug-api-tricks/
    // Notes:
    //  * ID3D11Device Refcount: 2 => This is a normal behavior as debugDevice still need m_Device to work
    //  * For any other object, Refcount: 0, We can ignore IntRef value
    hr = D3D11CreateDevice(adapter.Get(),
                           D3D_DRIVER_TYPE_UNKNOWN,
                           nullptr,
                           D3D11_CREATE_DEVICE_VIDEO_SUPPORT
#ifdef QT_DEBUG
                               | D3D11_CREATE_DEVICE_DEBUG
#endif
                           ,
                           supportedFeatureLevels,
                           ARRAYSIZE(supportedFeatureLevels),
                           D3D11_SDK_VERSION,
                           &m_Device,
                           &featureLevel,
                           &m_DeviceContext);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D11CreateDevice() failed: %x",
                     hr);
        goto Exit;
    }
    else if (adapterDesc.VendorId == 0x8086 && featureLevel <= D3D_FEATURE_LEVEL_11_0 && !qEnvironmentVariableIntValue("D3D11VA_ENABLED")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Avoiding D3D11VA on old pre-FL11.1 Intel GPU. Set D3D11VA_ENABLED=1 to override.");
        m_DeviceContext.Reset();
        m_Device.Reset();
        goto Exit;
    }
    else if (featureLevel >= D3D_FEATURE_LEVEL_11_0) {
        // Remember that we found a non-software D3D11 devices with support for
        // feature level 11.0 or later (Fermi, Terascale 2, or Ivy Bridge and later)
        m_DevicesWithFL11Support++;
    }

    // Avoid the application to crash in case of multithread conflict on the same resource
    if(SUCCEEDED(m_Device->QueryInterface(__uuidof(ID3D11Multithread), (void**)&pMultithread)))
    {
        pMultithread->SetMultithreadProtected(true);
    }

    if(m_VideoEnhancement->isVideoEnhancementEnabled() && !createVideoProcessor()){
        // Disable enhancement if the Video Processor creation failed
        m_VideoEnhancement->enableVideoEnhancement(false);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VideoProcessor failed to be created");
    }

    bool ok;
    m_BindDecoderOutputTextures = !!qEnvironmentVariableIntValue("D3D11VA_FORCE_BIND", &ok);
    if (!ok) {
        // Skip copying to our own internal texture on Intel GPUs due to
        // significant performance impact of the extra copy. See:
        // https://github.com/moonlight-stream/moonlight-qt/issues/1304
        m_BindDecoderOutputTextures = adapterDesc.VendorId == 0x8086;
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using D3D11VA_FORCE_BIND to override default bind/copy logic");
    }

    m_UseFenceHack = !!qEnvironmentVariableIntValue("D3D11VA_FORCE_FENCE", &ok);
    if (!ok) {
        // Old Intel GPUs (HD 4000) require a fence to properly synchronize
        // the video engine with the 3D engine for texture sampling.
        m_UseFenceHack = adapterDesc.VendorId == 0x8086 && featureLevel < D3D_FEATURE_LEVEL_11_1;
    }
    else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Using D3D11VA_FORCE_FENCE to override default fence workaround logic");
    }

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Decoder texture access: %s (fence: %s)",
                m_BindDecoderOutputTextures ? "bind" : "copy",
                (m_BindDecoderOutputTextures && m_UseFenceHack) ? "yes" : "no");

    // Check which fence types are supported by this GPU
    {
        m_FenceType = SupportedFenceType::None;

        ComPtr<IDXGIAdapter4> adapter4;
        if (SUCCEEDED(adapter.As(&adapter4))) {
            DXGI_ADAPTER_DESC3 desc3;
            if (SUCCEEDED(adapter4->GetDesc3(&desc3))) {
                if (desc3.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_MONITORED_FENCES) {
                    // Monitored fences must be used when they are supported
                    m_FenceType = SupportedFenceType::Monitored;
                }
                else if (desc3.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_NON_MONITORED_FENCES) {
                    // Non-monitored fences must only be used when monitored fences are unsupported
                    m_FenceType = SupportedFenceType::NonMonitored;
                }
            }
        }
    }

    if (!checkDecoderSupport(adapter.Get())) {
        m_DeviceContext.Reset();
        m_Device.Reset();
        if(m_VideoProcessorEnumerator){
            m_VideoProcessorEnumerator.Reset();
        }
        if(m_VideoProcessor){
            m_VideoProcessor.Reset();
        }

        goto Exit;
    }
    else {
        // Remember that we found a device with support for decoding this codec
        m_DevicesWithCodecSupport++;
    }

    success = true;

Exit:
    if (adapterNotFound != nullptr) {
        *adapterNotFound = !adapter;
    }
    return success;
}

/**
 * \brief Get the Adapter Index based on Video enhancement capabilities
 *
 * In case of multiple GPUs, get the most appropriate GPU available based on accessible capabilities
 * and priority of Vendor implementation status (NVIDIA -> AMD -> Intel -> Others).
 *
 * \return int Returns an Adapter index
 */
int D3D11VARenderer::getAdapterIndexByEnhancementCapabilities()
{
    ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 adapterDesc;

    int highestScore = -1;
    int adapterIndex = -1;
    int index = 0;
    while(m_Factory->EnumAdapters1(index, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND)
    {
        if (SUCCEEDED(adapter->GetDesc1(&adapterDesc))) {

            if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Skip the WARP device. We know it will fail.
                index++;
                continue;
            }

            m_DeviceContext.Reset();
            m_Device.Reset();
            if(m_VideoProcessorEnumerator){
                m_VideoProcessorEnumerator.Reset();
            }
            if(m_VideoProcessor){
                m_VideoProcessor.Reset();
            }

            if (SUCCEEDED(D3D11CreateDevice(
                    adapter.Get(),
                    D3D_DRIVER_TYPE_UNKNOWN,
                    nullptr,
                    D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                    nullptr,
                    0,
                    D3D11_SDK_VERSION,
                    &m_Device,
                    nullptr,
                    &m_DeviceContext))
                && createVideoProcessor()){

                // VSR has the priority over SDR-to-HDR in term of capability we want to use.
                // The priority value may change over the time,
                // below statement has been established based on drivers' capabilities status by February 29th 2024.

                int score = -1;

                // Video Super Resolution
                if(m_VideoEnhancement->isVendorAMD(adapterDesc.VendorId) && enableAMDVideoSuperResolution(false, false)){
                    score = std::max(score, 200);
                } else if(m_VideoEnhancement->isVendorIntel(adapterDesc.VendorId) && enableIntelVideoSuperResolution(false, false)){
                    score = std::max(score, 100);
                } else if(m_VideoEnhancement->isVendorNVIDIA(adapterDesc.VendorId) && enableNvidiaVideoSuperResolution(false, false)){
                    score = std::max(score, 300);
                }

                // SDR to HDR auto conversion
                if(m_VideoEnhancement->isVendorAMD(adapterDesc.VendorId) && enableAMDHDR(false, false)){
                    score = std::max(score, 20);
                } else if(m_VideoEnhancement->isVendorIntel(adapterDesc.VendorId) && enableIntelHDR(false, false)){
                    score = std::max(score, 10);
                } else if(m_VideoEnhancement->isVendorNVIDIA(adapterDesc.VendorId) && enableNvidiaHDR(false, false)){
                    score = std::max(score, 30);
                }

                // Recording the highest score, which will represent the most capable adapater for Video enhancement
                if(score > highestScore){
                    highestScore = score;
                    adapterIndex = index;
                }
            }
        }

        index++;
    }

    // Set Video enhancement information
    if(m_Factory->EnumAdapters1(adapterIndex, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND){

        if (SUCCEEDED(adapter->GetDesc1(&adapterDesc))) {

            m_DeviceContext.Reset();
            m_Device.Reset();
            if(m_VideoProcessorEnumerator){
                m_VideoProcessorEnumerator.Reset();
            }
            if(m_VideoProcessor){
                m_VideoProcessor.Reset();
            }

            if (SUCCEEDED(D3D11CreateDevice(
                    adapter.Get(),
                    D3D_DRIVER_TYPE_UNKNOWN,
                    nullptr,
                    D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                    nullptr,
                    0,
                    D3D11_SDK_VERSION,
                    &m_Device,
                    nullptr,
                    &m_DeviceContext))
                && createVideoProcessor()){

                m_VideoEnhancement->setVendorID(adapterDesc.VendorId);

                if(adapterIndex >= 0){
                    // Convert wchar[128] to string
                    std::wstring GPUname(adapterDesc.Description);
                    qInfo() << "GPU used for Video Enhancement: " << GPUname;

                    // Test, but do not active yet to ensure it will be reinitialize when needed
                    if(m_VideoEnhancement->isVendorAMD()){
                        m_VideoEnhancement->setVSRcapable(enableAMDVideoSuperResolution(false));
                        m_VideoEnhancement->setHDRcapable(enableAMDHDR(false));
                    } else if(m_VideoEnhancement->isVendorIntel()){
                        m_VideoEnhancement->setVSRcapable(enableIntelVideoSuperResolution(false));
                        m_VideoEnhancement->setHDRcapable(enableIntelHDR(false));
                    } else if(m_VideoEnhancement->isVendorNVIDIA()){
                        m_VideoEnhancement->setVSRcapable(enableNvidiaVideoSuperResolution(false));
                        m_VideoEnhancement->setHDRcapable(enableNvidiaHDR(false));
                    } else if (m_VideoProcessorCapabilities.AutoStreamCaps & D3D11_VIDEO_PROCESSOR_AUTO_STREAM_CAPS_SUPER_RESOLUTION){
                        // Try Auto Stream Super Resolution provided by DirectX11+ and agnostic to any Vendor
                        m_AutoStreamSuperResolution = true;
                        m_VideoEnhancement->setVSRcapable(true);
                    }

                    // Enable the visibility of Video enhancement feature in the settings of the User interface
                    m_VideoEnhancement->enableUIvisible();
                }
            }
        }
    }

    m_DeviceContext.Reset();
    m_Device.Reset();
    if(m_VideoProcessorEnumerator){
        m_VideoProcessorEnumerator.Reset();
    }
    if(m_VideoProcessor){
        m_VideoProcessor.Reset();
    }

    return adapterIndex;
}

/**
 * \brief Enable Video Super-Resolution for AMD GPU
 *
 * This feature is available since this drive 22.3.1 (March 2022)
 * https://community.amd.com/t5/gaming/amd-software-24-1-1-amd-fluid-motion-frames-an-updated-ui-and/ba-p/656213
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableAMDVideoSuperResolution(bool activate, bool logInfo){
    // The feature is announced since Jan 23rd, 2024, with the driver 24.1.1 and on series 7000
    // https://community.amd.com/t5/gaming/amd-software-24-1-1-amd-fluid-motion-frames-an-updated-ui-and/ba-p/656213
    // But it is available as SDK since March 2022 (22.3.1) which means it might also work for series 5000 and 6000 (to be tested)
    // https://github.com/GPUOpen-LibrariesAndSDKs/AMF/blob/master/amf/doc/AMF_HQ_Scaler_API.md

    AMF_RESULT res;
    amf::AMFCapsPtr amfCaps;
    amf::AMFIOCapsPtr pInputCaps;

    // We skip if already initialized
    if(m_AmfInitialized && activate)
        return true;

    amf::AMF_SURFACE_FORMAT SurfaceFormatYUV;
    AMFColor backgroundColor = AMFConstructColor(0, 0, 0, 255);

    // AMF Context initialization
    res = g_AMFFactory.Init();
    if (res != AMF_OK) goto Error;
    res = g_AMFFactory.GetFactory()->CreateContext(&m_AmfContext);
    if (res != AMF_OK) goto Error;
    res = g_AMFFactory.GetFactory()->CreateComponent(m_AmfContext, AMFHQScaler, &m_AmfUpScaler);
    if (res != AMF_OK) goto Error;

    res = m_AmfContext->InitDX11(m_Device.Get());
    if (res != AMF_OK) goto Error;

    // AMFHQScaler is the newest feature available (v1.4.33), so at least this one need to be accessible
    m_AmfUpScaler->GetCaps(&amfCaps);
    if (amfCaps != nullptr && amfCaps->GetAccelerationType() == amf::AMF_ACCEL_NOT_SUPPORTED) {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "The hardware does not support needed AMD AMF capabilities.");
        goto Error;
    }

    // Format initialization
    SurfaceFormatYUV = (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT) ? amf::AMF_SURFACE_P010 : amf::AMF_SURFACE_NV12;

    // Input Surface initialization
    res = m_AmfContext->AllocSurface(amf::AMF_MEMORY_DX11,
                                     SurfaceFormatYUV,
                                     m_DecoderParams.width,
                                     m_DecoderParams.height,
                                     &m_AmfSurface);
    if (res != AMF_OK) goto Error;

    // Upscale initialization
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_DisplayWidth, m_DisplayHeight));
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, amf::AMF_MEMORY_DX11);
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_ALGORITHM, AMF_HQ_SCALER_ALGORITHM_VIDEOSR1_0);
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_KEEP_ASPECT_RATIO, true);
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_FILL, true);
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_FILL_COLOR, backgroundColor);
    // We only apply sharpening when the picture is scaled (0 = Most sharpened / 2.00 = Not sharpened)
    if (m_OutputTexture.width == m_DecoderParams.width && m_OutputTexture.height == m_DecoderParams.height){
        m_AmfUpScalerSharpness = false;
    } else {
        m_AmfUpScalerSharpness = true;
    }
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_SHARPNESS, m_AmfUpScalerSharpness ? 0.30 : 2.00);
    m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_FRAME_RATE, m_DecoderParams.frameRate);
    // Initialize with the size of the texture that will be input
    m_AmfUpScalerSurfaceFormat = SurfaceFormatYUV;
    res = m_AmfUpScaler->Init(SurfaceFormatYUV,
                              m_DecoderParams.width,
                              m_DecoderParams.height);
    if (res != AMF_OK) goto Error;

    if(!activate){
        // Up Scaler
        m_AmfUpScaler->Terminate();
        m_AmfUpScaler = nullptr;
        // Context
        m_AmfContext->Terminate();
        m_AmfContext = nullptr;
        // Factory
        g_AMFFactory.Terminate();

        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD Video Super Resolution disabled");
    } else {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD Video Super Resolution enabled");
    }

    m_AmfInitialized = activate;
    return true;

Error:
    if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD Video Super Resolution failed.");
    m_AmfInitialized = false;
    return false;
}

/**
 * \brief Enable Video Super-Resolution for Intel GPU
 *
 * This experimental feature from Intel is available starting from Intel iGPU from CPU Gen 10th (Skylake) and Intel graphics driver 27.20.100.8681 (Sept 15, 2020)
 * Only Arc GPUs seem to provide visual improvement
 * https://www.techpowerup.com/305558/intel-outs-video-super-resolution-for-chromium-browsers-works-with-igpus-11th-gen-onward
 * Values from Chromium source code:
 * https://chromium.googlesource.com/chromium/src/+/master/ui/gl/swap_chain_presenter.cc
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableIntelVideoSuperResolution(bool activate, bool logInfo){
    HRESULT hr;

    constexpr GUID GUID_INTEL_VPE_INTERFACE = {0xedd1d4b9, 0x8659, 0x4cbc, {0xa4, 0xd6, 0x98, 0x31, 0xa2, 0x16, 0x3a, 0xc3}};
    constexpr UINT kIntelVpeFnVersion = 0x01;
    constexpr UINT kIntelVpeFnMode = 0x20;
    constexpr UINT kIntelVpeFnScaling = 0x37;
    constexpr UINT kIntelVpeVersion3 = 0x0003;
    constexpr UINT kIntelVpeModeNone = 0x0;
    constexpr UINT kIntelVpeModePreproc = 0x01;
    constexpr UINT kIntelVpeScalingDefault = 0x0;
    constexpr UINT kIntelVpeScalingSuperResolution = 0x2;

    UINT param = 0;

    struct IntelVpeExt
    {
        UINT function;
        void* param;
    };

    IntelVpeExt ext{0, &param};

    ext.function = kIntelVpeFnVersion;
    param = kIntelVpeVersion3;

    hr = m_VideoContext->VideoProcessorSetOutputExtension(
        m_VideoProcessor.Get(), &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
    if (FAILED(hr))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Intel VPE version failed: %x",
                     hr);
        return false;
    }

    ext.function = kIntelVpeFnMode;
    if(activate){
        param = kIntelVpeModePreproc;
    } else {
        param = kIntelVpeModeNone;
    }

    hr = m_VideoContext->VideoProcessorSetOutputExtension(
        m_VideoProcessor.Get(), &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
    if (FAILED(hr))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Intel VPE mode failed: %x",
                     hr);
        return false;
    }

    ext.function = kIntelVpeFnScaling;
    if(activate){
        param = kIntelVpeScalingSuperResolution;
    } else {
        param = kIntelVpeScalingDefault;
    }

    hr = m_VideoContext->VideoProcessorSetStreamExtension(
        m_VideoProcessor.Get(), 0, &GUID_INTEL_VPE_INTERFACE, sizeof(ext), &ext);
    if (FAILED(hr))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Intel Video Super Resolution failed: %x",
                     hr);
        return false;
    }

    if(activate){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution enabled");
    } else {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution disabled");
    }

    return true;
}

/**
 * \brief Enable Video Super-Resolution for NVIDIA
 *
 * This feature is available starting from series NVIDIA RTX 2000 and GeForce driver 545.84 (Oct 17, 2023)
 *
 * IMPORTANT (Feb 5th, 2024): RTX VSR seems to be limited to SDR content only,
 * it does add a grey filter if it is activated while HDR is on on stream (Host setting does not impact it).
 * It might be fixed later by NVIDIA, but the temporary solution is to disable the feature when Stream content is HDR-on
 * Values from Chromium source code:
 * https://chromium.googlesource.com/chromium/src/+/master/ui/gl/swap_chain_presenter.cc
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableNvidiaVideoSuperResolution(bool activate, bool logInfo){
    HRESULT hr;

    // Toggle VSR
    constexpr GUID GUID_NVIDIA_PPE_INTERFACE = {0xd43ce1b3, 0x1f4b, 0x48ac, {0xba, 0xee, 0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7}};
    constexpr UINT kStreamExtensionVersionV1 = 0x1;
    constexpr UINT kStreamExtensionMethodSuperResolution = 0x2;

    struct NvidiaStreamExt
    {
        UINT version;
        UINT method;
        UINT enable;
    };

    // Convert bool to UINT
    UINT enable = activate;

    NvidiaStreamExt stream_extension_info = {kStreamExtensionVersionV1, kStreamExtensionMethodSuperResolution, enable};
    hr = m_VideoContext->VideoProcessorSetStreamExtension(m_VideoProcessor.Get(), 0, &GUID_NVIDIA_PPE_INTERFACE, sizeof(stream_extension_info), &stream_extension_info);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "NVIDIA RTX Video Super Resolution failed: %x",
                     hr);
        return false;
    }

    if(activate){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution enabled");
    } else {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution disabled");
    }

    return true;
}

/**
 * \brief Enable HDR for AMD GPU
 *
 * This feature is not availble for AMD, and has not yet been announced (by Jan 24th, 2024)
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableAMDHDR(bool activate, bool logInfo){

    // [TODO] Feature not yet announced

    if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD HDR capability is not yet supported by your client's GPU.");
    return false;
}

/**
 * \brief Enable HDR for Intel GPU
 *
 * This feature is not availble for Intel, and has not yet been announced (by Jan 24th, 2024)
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableIntelHDR(bool activate, bool logInfo){

    // [TODO] Feature not yet announced

    if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel HDR capability is not yet supported by your client's GPU.");
    return false;
}

/**
 * \brief Enable HDR for NVIDIA
 *
 * This feature is available starting from series NVIDIA RTX 2000 and GeForce driver 545.84 (Oct 17, 2023)
 *
 * Note: Even if the feature is enabled, I could not find any settings of ColorSpace and DXG8Format which
 * can work without having the screen darker. Here are what I found:
 *  1) Moonlight HDR: Checked / SwapChain: DXGI_FORMAT_R10G10B10A2_UNORM / VideoTexture: DXGI_FORMAT_P010 => SDR convert to HDR, but with darker rendering
 *  2) Moonlight HDR: Unchecked / SwapChain: DXGI_FORMAT_R10G10B10A2_UNORM / VideoTexture: DXGI_FORMAT_NV12 => SDR convert to HDR, but with darker rendering
 * Values from Chromium source code:
 * https://chromium.googlesource.com/chromium/src/+/master/ui/gl/swap_chain_presenter.cc
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \return bool Return true if the capability is available
 */
bool D3D11VARenderer::enableNvidiaHDR(bool activate, bool logInfo){
    HRESULT hr;

    // Toggle HDR
    constexpr GUID GUID_NVIDIA_TRUE_HDR_INTERFACE = {0xfdd62bb4, 0x620b, 0x4fd7, {0x9a, 0xb3, 0x1e, 0x59, 0xd0, 0xd5, 0x44, 0xb3}};
    constexpr UINT kStreamExtensionVersionV4 = 0x4;
    constexpr UINT kStreamExtensionMethodTrueHDR = 0x3;

    struct NvidiaStreamExt
    {
        UINT version;
        UINT method;
        UINT enable : 1;
        UINT reserved : 31;
    };

    // Convert bool to UINT
    UINT enable = activate;

    NvidiaStreamExt stream_extension_info = {kStreamExtensionVersionV4, kStreamExtensionMethodTrueHDR, enable, 0u};
    hr = m_VideoContext->VideoProcessorSetStreamExtension(m_VideoProcessor.Get(), 0, &GUID_NVIDIA_TRUE_HDR_INTERFACE, sizeof(stream_extension_info), &stream_extension_info);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "NVIDIA RTX HDR failed: %x",
                     hr);
        return false;
    }

    if(activate){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX HDR enabled");
    } else {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX HDR disabled");
    }

    return true;
}

bool D3D11VARenderer::initialize(PDECODER_PARAMETERS params)
{
    HRESULT hr;

    m_DecoderParams = *params;

    // Use only even number to avoid a crash a texture creation
    m_DecoderParams.width = m_DecoderParams.width & ~1;
    m_DecoderParams.height = m_DecoderParams.height & ~1;

    if (qgetenv("D3D11VA_ENABLED") == "0") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "D3D11VA is disabled by environment variable");
        return false;
    }
    else if (!IsWindows10OrGreater()) {
        // Use DXVA2 on anything older than Win10, so we don't have to handle a bunch
        // of legacy Win7/Win8 codepaths in here.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D11VA renderer is only supported on Windows 10 or later.");
        return false;
    }

    // By default try the adapter corresponding to the display where our window resides.
    // This will let us avoid a copy if the display GPU has the required decoder.
    // If Video enhancement is enabled, it will look for the most capable GPU in case of multiple GPUs.
    if (!SDL_DXGIGetOutputInfo(SDL_GetWindowDisplayIndex(params->window),
                               &m_AdapterIndex, &m_OutputIndex)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_DXGIGetOutputInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    // Use the current window size as the swapchain size
    SDL_GetWindowSize(m_DecoderParams.window, (int*)&m_DisplayWidth, (int*)&m_DisplayHeight);

    // Rounddown to even number to avoid a crash at texture creation
    m_DisplayWidth = m_DisplayWidth & ~1;
    m_DisplayHeight = m_DisplayHeight & ~1;

    // As m_Display corresponds to the application window, which may not have the same ratio as the Frame,
    // we calculate the size of the final texture to fit in the window without distortion
    m_OutputTexture.width = m_DisplayWidth;
    m_OutputTexture.height = m_DisplayHeight;
    m_OutputTexture.left = 0;
    m_OutputTexture.top = 0;

    // Sscale the source to the destination surface while keeping the same ratio
    float ratioWidth = static_cast<float>(m_DisplayWidth) / static_cast<float>(m_DecoderParams.width);
    float ratioHeight = static_cast<float>(m_DisplayHeight) / static_cast<float>(m_DecoderParams.height);

    if(ratioHeight < ratioWidth){
        // Adjust the Width
        m_OutputTexture.width = static_cast<int>(std::floor(m_DecoderParams.width * ratioHeight));
        m_OutputTexture.width = m_OutputTexture.width & ~1;
        m_OutputTexture.left = static_cast<int>(std::floor(  abs(m_DisplayWidth - m_OutputTexture.width) / 2  ));
        m_OutputTexture.left = m_OutputTexture.left & ~1;
    } else if(ratioWidth < ratioHeight) {
        // Adjust the Height
        m_OutputTexture.height = static_cast<int>(std::floor(m_DecoderParams.height * ratioWidth));
        m_OutputTexture.height = m_OutputTexture.height & ~1;
        m_OutputTexture.top = static_cast<int>(std::floor(  abs(m_DisplayHeight - m_OutputTexture.height) / 2  ));
        m_OutputTexture.top = m_OutputTexture.top & ~1;
    }

    hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)m_Factory.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateDXGIFactory() failed: %x",
                     hr);
        return false;
    }

    // If getAdapterIndex return 0+, it means that we already identified which adapter best fit for Video enhancement,
    // so we don't have to estimate it more times to speed up the launch of the streaming.
    if(m_VideoEnhancement->getAdapterIndex() < 0){
        // This line is run only once during the application life and is necessary to display (or not)
        // the Video enhancement checkbox if the GPU enables it
        int adapterIndex = getAdapterIndexByEnhancementCapabilities();
        if(adapterIndex >= 0){
            m_VideoEnhancement->setAdapterIndex(adapterIndex);
        } else {
            m_VideoEnhancement->setAdapterIndex(m_AdapterIndex);
        }
    }

    if(m_VideoEnhancement->isEnhancementCapable()){
        // Check if the user has enable Video enhancement
        m_VideoEnhancement->enableVideoEnhancement(m_DecoderParams.enableVideoEnhancement);
    }

    // Set the adapter index of the most appropriate GPU
    if(
        m_VideoEnhancement->isVideoEnhancementEnabled()
        && m_VideoEnhancement->getAdapterIndex() >= 0
        ){
        m_AdapterIndex = m_VideoEnhancement->getAdapterIndex();
    }
    if (!createDeviceByAdapterIndex(m_AdapterIndex)) {
        // If that didn't work, we'll try all GPUs in order until we find one
        // or run out of GPUs (DXGI_ERROR_NOT_FOUND from EnumAdapters())
        bool adapterNotFound = false;
        for (int i = 0; !adapterNotFound; i++) {
            if (i == m_AdapterIndex) {
                // Don't try the same GPU again
                continue;
            }

            if (createDeviceByAdapterIndex(i, &adapterNotFound)) {
                // This GPU worked! Continue initialization.
                break;
            }
        }

        if (adapterNotFound) {
            SDL_assert(!m_Device);
            SDL_assert(!m_DeviceContext);
            return false;
        }
    }

    if(m_BindDecoderOutputTextures){
        // Disable Video enhancement as we do not copy the frame to process it
        m_VideoEnhancement->enableVideoEnhancement(false);
    }

    // Set VSR and HDR
    if(m_VideoEnhancement->isVideoEnhancementEnabled()){
        // Enable VSR feature if available
        if(m_VideoEnhancement->isVSRcapable()){
            // Try Auto Stream Super Resolution provided by DirectX11+ and agnostic to any Vendor
            if (m_AutoStreamSuperResolution){
                // The flag does exist, but not the method yet (by March 8th, 2024)
                // We still can prepare the code once Microsof enable it.
                // m_VideoContext->VideoProcessorSetStreamSuperResolution(m_VideoProcessor.Get(), 0, true);
            } else if(m_VideoEnhancement->isVendorAMD()){
                enableAMDVideoSuperResolution();
            } else if(m_VideoEnhancement->isVendorIntel()){
                enableIntelVideoSuperResolution();
            } else if(m_VideoEnhancement->isVendorNVIDIA()){
                enableNvidiaVideoSuperResolution();
            }
        }

        // Enable SDR->HDR feature if available
        if(m_VideoEnhancement->isHDRcapable()){
            if(m_VideoEnhancement->isVendorAMD()){
                enableAMDHDR();
            } else if(m_VideoEnhancement->isVendorIntel()){
                enableIntelHDR();
            } else if(m_VideoEnhancement->isVendorNVIDIA()){
                if(m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT){
                    // Disable SDR->HDR feature because the screen becomes grey when activated
                    enableNvidiaHDR(false);
                } else {
                    enableNvidiaHDR();
                }
            }
        }
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    // 3 front buffers (default GetMaximumFrameLatency() count)
    // + 1 back buffer
    // + 1 extra for DWM to hold on to for DirectFlip
    //
    // Even though we allocate 3 front buffers for pre-rendered frames,
    // they won't actually increase presentation latency because we
    // always use SyncInterval 0 which replaces the last one.
    //
    // IDXGIDevice1 has a SetMaximumFrameLatency() function, but counter-
    // intuitively we must avoid it to reduce latency. If we set our max
    // frame latency to 1 on thedevice, our SyncInterval 0 Present() calls
    // will block on DWM (acting like SyncInterval 1) rather than doing
    // the non-blocking present we expect.
    //
    // NB: 3 total buffers seems sufficient on NVIDIA hardware but
    // causes performance issues (buffer starvation) on AMD GPUs.
    swapChainDesc.BufferCount = 3 + 1 + 1;

    swapChainDesc.Width = m_DisplayWidth;
    swapChainDesc.Height = m_DisplayHeight;
    if (params->videoFormat & VIDEO_FORMAT_MASK_10BIT) {
        swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    }
    else {
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    // Use DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING with flip mode for non-vsync case, if possible.
    // NOTE: This is only possible in windowed or borderless windowed mode.
    if (!params->enableVsync) {
        BOOL allowTearing = FALSE;
        hr = m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                            &allowTearing,
                                            sizeof(allowTearing));
        if (SUCCEEDED(hr)) {
            if (allowTearing) {
                // Use flip discard with allow tearing mode if possible.
                swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                m_AllowTearing = true;
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "OS/GPU doesn't support DXGI_FEATURE_PRESENT_ALLOW_TEARING");
            }
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "IDXGIFactory::CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING) failed: %x",
                         hr);
            // Non-fatal
        }

        // DXVA2 may let us take over for FSE V-sync off cases. However, if we don't have DXGI_FEATURE_PRESENT_ALLOW_TEARING
        // then we should not attempt to do this unless there's no other option (HDR, DXVA2 failed in pass 1, etc).
        if (!m_AllowTearing && m_DecoderSelectionPass == 0 && !(params->videoFormat & VIDEO_FORMAT_MASK_10BIT) &&
            (SDL_GetWindowFlags(params->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Defaulting to DXVA2 for FSE without DXGI_FEATURE_PRESENT_ALLOW_TEARING support");
            return false;
        }
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(params->window, &info);
    SDL_assert(info.subsystem == SDL_SYSWM_WINDOWS);

    // Always use windowed or borderless windowed mode.. SDL does mode-setting for us in
    // full-screen exclusive mode (SDL_WINDOW_FULLSCREEN), so this actually works out okay.
    ComPtr<IDXGISwapChain1> swapChain;
    hr = m_Factory->CreateSwapChainForHwnd(m_Device.Get(),
                                           info.info.win.window,
                                           &swapChainDesc,
                                           nullptr,
                                           nullptr,
                                           swapChain.GetAddressOf());

    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIFactory::CreateSwapChainForHwnd() failed: %x",
                     hr);
        return false;
    }

    hr = swapChain.As(&m_SwapChain);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGISwapChain::QueryInterface(IDXGISwapChain4) failed: %x",
                     hr);
        return false;
    }

    // Disable Alt+Enter, PrintScreen, and window message snooping. This makes
    // it safe to run the renderer on a separate rendering thread rather than
    // requiring the main (message loop) thread.
    hr = m_Factory->MakeWindowAssociation(info.info.win.window, DXGI_MWA_NO_WINDOW_CHANGES);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIFactory::MakeWindowAssociation() failed: %x",
                     hr);
        return false;
    }

    {
        m_HwDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!m_HwDeviceContext) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to allocate D3D11VA device context");
            return false;
        }

        AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwDeviceContext->data;
        AVD3D11VADeviceContext* d3d11vaDeviceContext = (AVD3D11VADeviceContext*)deviceContext->hwctx;

        // FFmpeg will take ownership of these pointers, so we use CopyTo() to bump the ref count
        m_Device.CopyTo(&d3d11vaDeviceContext->device);
        m_DeviceContext.CopyTo(&d3d11vaDeviceContext->device_context);

        // Set lock functions that we will use to synchronize with FFmpeg's usage of our device context
        d3d11vaDeviceContext->lock = lockContext;
        d3d11vaDeviceContext->unlock = unlockContext;
        d3d11vaDeviceContext->lock_ctx = this;

        int err = av_hwdevice_ctx_init(m_HwDeviceContext);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to initialize D3D11VA device context: %d",
                         err);
            return false;
        }
    }

    // Surfaces must be 16 pixel aligned for H.264 and 128 pixel aligned for everything else
    // https://github.com/FFmpeg/FFmpeg/blob/a234e5cd80224c95a205c1f3e297d8c04a1374c3/libavcodec/dxva2.c#L609-L616
    m_TextureAlignment = (params->videoFormat & VIDEO_FORMAT_MASK_H264) ? 16 : 128;

    {
        m_HwFramesContext = av_hwframe_ctx_alloc(m_HwDeviceContext);
        if (!m_HwFramesContext) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to allocate D3D11VA frame context");
            return false;
        }

        AVHWFramesContext* framesContext = (AVHWFramesContext*)m_HwFramesContext->data;

        framesContext->format = AV_PIX_FMT_D3D11;
        if (params->videoFormat & VIDEO_FORMAT_MASK_10BIT) {
            framesContext->sw_format = (params->videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
                                           AV_PIX_FMT_XV30 : AV_PIX_FMT_P010;
        }
        else {
            framesContext->sw_format = (params->videoFormat & VIDEO_FORMAT_MASK_YUV444) ?
                                           AV_PIX_FMT_VUYX : AV_PIX_FMT_NV12;
        }

        framesContext->width = FFALIGN(params->width, m_TextureAlignment);
        framesContext->height = FFALIGN(params->height, m_TextureAlignment);

        // We can have up to 16 reference frames plus a working surface
        framesContext->initial_pool_size = DECODER_BUFFER_POOL_SIZE;

        AVD3D11VAFramesContext* d3d11vaFramesContext = (AVD3D11VAFramesContext*)framesContext->hwctx;

        d3d11vaFramesContext->BindFlags = D3D11_BIND_DECODER;
        if (m_BindDecoderOutputTextures) {
            // We need to override the default D3D11VA bind flags to bind the textures as a shader resources
            d3d11vaFramesContext->BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }

        int err = av_hwframe_ctx_init(m_HwFramesContext);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to initialize D3D11VA frame context: %d",
                         err);
            return false;
        }

        D3D11_TEXTURE2D_DESC textureDesc;
        d3d11vaFramesContext->texture_infos->texture->GetDesc(&textureDesc);
        m_TextureFormat = textureDesc.Format;

        if (m_BindDecoderOutputTextures) {
            // Create SRVs for all textures in the decoder pool
            if (!setupTexturePoolViews(d3d11vaFramesContext)) {
                return false;
            }
        }
        else {
            // Create our internal texture to copy and render
            if (!setupVideoTexture()) {
                return false;
            }
        }
    }

    m_SrcBox.left = 0;
    m_SrcBox.top = 0;
    m_SrcBox.right = m_DecoderParams.width;
    m_SrcBox.bottom = m_DecoderParams.height;
    m_SrcBox.front = 0;
    m_SrcBox.back = 1;

    // Create our video textures and SRVs
    if (!setupEnhancedTexture() || !setupAmfTexture()) {
        return false;
    }

    // As for Video Enhancement, the RTV uses a texture, it needs to be setup after the textures creation
    if (!setupRenderingResources()) {
        return false;
    }

    if(m_VideoProcessor && m_VideoEnhancement->isVideoEnhancementEnabled()){
        initializeVideoProcessor();
    }

    return true;
}

bool D3D11VARenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary**)
{
    context->hw_device_ctx = av_buffer_ref(m_HwDeviceContext);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Using D3D11VA accelerated renderer");

    return true;
}

bool D3D11VARenderer::prepareDecoderContextInGetFormat(AVCodecContext *context, AVPixelFormat)
{
    // hw_frames_ctx must be initialized in ffGetFormat().
    context->hw_frames_ctx = av_buffer_ref(m_HwFramesContext);

    return true;
}

void D3D11VARenderer::renderFrame(AVFrame* frame)
{
    // Acquire the context lock for rendering to prevent concurrent
    // access from inside FFmpeg's decoding code
    lockContext(this);

    // Clear the back buffer
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_DeviceContext->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);

    // Bind the back buffer. This needs to be done each time,
    // because the render target view will be unbound by Present().
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), nullptr);

    // Prepare the Enhanced Output
    if(m_VideoEnhancement->isVideoEnhancementEnabled()){
        prepareEnhancedOutput(frame);
    }

    // Render our video frame with the aspect-ratio adjusted viewport
    renderVideo(frame);

    // Render overlays on top of the video stream
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        renderOverlay((Overlay::OverlayType)i);
    }

    UINT flags;

    if (m_AllowTearing) {
        SDL_assert(!m_DecoderParams.enableVsync);

        // If tearing is allowed, use DXGI_PRESENT_ALLOW_TEARING with syncInterval 0.
        // It is not valid to use any other syncInterval values in tearing mode.
        flags = DXGI_PRESENT_ALLOW_TEARING;
    }
    else {
        // Otherwise, we'll submit as fast as possible and DWM will discard excess
        // frames for us. If frame pacing is also enabled or we're in full-screen,
        // our Vsync source will keep us in sync with VBlank.
        flags = 0;
    }

    HRESULT hr;

    if (frame->color_trc != m_LastColorTrc) {
        if (frame->color_trc == AVCOL_TRC_SMPTE2084) {
            // Switch to Rec 2020 PQ (SMPTE ST 2084) colorspace for HDR10 rendering
            hr = m_SwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "IDXGISwapChain::SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) failed: %x",
                             hr);
            }
        }
        else {
            // Restore default sRGB colorspace
            hr = m_SwapChain->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "IDXGISwapChain::SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709) failed: %x",
                             hr);
            }
        }

        m_LastColorTrc = frame->color_trc;
    }

    // Present according to the decoder parameters
    hr = m_SwapChain->Present(0, flags);

    // Release the context lock
    unlockContext(this);

    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGISwapChain::Present() failed: %x",
                     hr);

        // The card may have been removed or crashed. Reset the decoder.
        SDL_Event event;
        event.type = SDL_RENDER_TARGETS_RESET;
        SDL_PushEvent(&event);
        return;
    }
}

void D3D11VARenderer::renderOverlay(Overlay::OverlayType type)
{
    if (!Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        return;
    }

    // If the overlay is being updated, just skip rendering it this frame
    if (!SDL_AtomicTryLock(&m_OverlayLock)) {
        return;
    }

    // Reference these objects so they don't immediately go away if the
    // overlay update thread tries to release them.
    ComPtr<ID3D11Texture2D> overlayTexture = m_OverlayTextures[type];
    ComPtr<ID3D11Buffer> overlayVertexBuffer = m_OverlayVertexBuffers[type];
    ComPtr<ID3D11ShaderResourceView> overlayTextureResourceView = m_OverlayTextureResourceViews[type];
    SDL_AtomicUnlock(&m_OverlayLock);

    if (!overlayTexture) {
        return;
    }

    // If there was a texture, there must also be a vertex buffer and SRV
    SDL_assert(overlayVertexBuffer);
    SDL_assert(overlayTextureResourceView);

    // Bind vertex buffer
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, overlayVertexBuffer.GetAddressOf(), &stride, &offset);

    // Bind pixel shader and resources
    m_DeviceContext->PSSetShader(m_OverlayPixelShader.Get(), nullptr, 0);
    m_DeviceContext->PSSetShaderResources(0, 1, overlayTextureResourceView.GetAddressOf());

    // Draw the overlay
    m_DeviceContext->DrawIndexed(6, 0, 0);
}

void D3D11VARenderer::bindColorConversion(AVFrame* frame)
{
    bool fullRange = isFrameFullRange(frame);
    int colorspace = getFrameColorspace(frame);
    bool yuv444 = (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_YUV444);

    // We have purpose-built shaders for the common Rec 601 (SDR) and Rec 2020 (HDR) YUV 4:2:0 cases
    if (!yuv444 && !fullRange && colorspace == COLORSPACE_REC_601) {
        m_DeviceContext->PSSetShader(m_VideoPixelShaders[PixelShaders::BT_601_LIMITED_YUV_420].Get(), nullptr, 0);
    }
    else if (!yuv444 && !fullRange && colorspace == COLORSPACE_REC_2020) {
        m_DeviceContext->PSSetShader(m_VideoPixelShaders[PixelShaders::BT_2020_LIMITED_YUV_420].Get(), nullptr, 0);
    }
    else {
        if (yuv444) {
            // We'll need to use one of the 4:4:4 shaders for this pixel format
            switch (m_TextureFormat)
            {
            case DXGI_FORMAT_AYUV:
                m_DeviceContext->PSSetShader(m_VideoPixelShaders[PixelShaders::GENERIC_AYUV].Get(), nullptr, 0);
                break;
            case DXGI_FORMAT_Y410:
                m_DeviceContext->PSSetShader(m_VideoPixelShaders[PixelShaders::GENERIC_Y410].Get(), nullptr, 0);
                break;
            default:
                SDL_assert(false);
            }
        }
        else {
            // We'll need to use the generic 4:2:0 shader for this colorspace and color range combo
            m_DeviceContext->PSSetShader(m_VideoPixelShaders[PixelShaders::GENERIC_YUV_420].Get(), nullptr, 0);
        }

        // If nothing has changed since last frame, we're done
        if (colorspace == m_LastColorSpace && fullRange == m_LastFullRange) {
            return;
        }

        if (!yuv444) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Falling back to generic video pixel shader for %d (%s range)",
                        colorspace,
                        fullRange ? "full" : "limited");
        }

        D3D11_BUFFER_DESC constDesc = {};
        constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
        constDesc.Usage = D3D11_USAGE_IMMUTABLE;
        constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constDesc.CPUAccessFlags = 0;
        constDesc.MiscFlags = 0;

        CSC_CONST_BUF constBuf = {};
        const float* rawCscMatrix;
        switch (colorspace) {
        case COLORSPACE_REC_601:
            rawCscMatrix = fullRange ? k_CscMatrix_Bt601Full : k_CscMatrix_Bt601Lim;
            break;
        case COLORSPACE_REC_709:
            rawCscMatrix = fullRange ? k_CscMatrix_Bt709Full : k_CscMatrix_Bt709Lim;
            break;
        case COLORSPACE_REC_2020:
            rawCscMatrix = fullRange ? k_CscMatrix_Bt2020Full : k_CscMatrix_Bt2020Lim;
            break;
        default:
            SDL_assert(false);
            return;
        }

        // We need to adjust our raw CSC matrix to be column-major and with float3 vectors
        // padded with a float in between each of them to adhere to HLSL requirements.
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                constBuf.cscMatrix[i * 4 + j] = rawCscMatrix[j * 3 + i];
            }
        }

        // No adjustments are needed to the float[3] array of offsets, so it can just
        // be copied with memcpy().
        memcpy(constBuf.offsets,
               fullRange ? k_Offsets_Full : k_Offsets_Lim,
               sizeof(constBuf.offsets));

        D3D11_SUBRESOURCE_DATA constData = {};
        constData.pSysMem = &constBuf;

        ComPtr<ID3D11Buffer> constantBuffer;
        HRESULT hr = m_Device->CreateBuffer(&constDesc, &constData, constantBuffer.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->PSSetConstantBuffers(1, 1, constantBuffer.GetAddressOf());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBuffer() failed: %x",
                         hr);
            return;
        }
    }

    m_LastColorSpace = colorspace;
    m_LastFullRange = fullRange;
}

/**
 * \brief Set the output for enhanced rendering
 *
 * According to the colorspace from the source, set the corresponding output colorspace.
 * For AMF, disable the sharpness when HDR is on on Host
 *
 * \param AVFrame* frame The frame to be displayed on screen
 * \return void
 */
void D3D11VARenderer::prepareEnhancedOutput(AVFrame* frame)
{
    bool frameFullRange = isFrameFullRange(frame);
    int frameColorSpace = getFrameColorspace(frame);

    // If nothing has changed since last frame, we're done
    if (frameColorSpace == m_LastColorSpace && frameFullRange == m_LastFullRange) {
        return;
    }

    m_LastColorSpace = frameColorSpace;
    m_LastFullRange = frameFullRange;

    switch (frameColorSpace) {

    case COLORSPACE_REC_2020:
        m_VideoContext->VideoProcessorSetStreamColorSpace1(m_VideoProcessor.Get(), 0, frameFullRange ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020);
        m_VideoContext->VideoProcessorSetOutputColorSpace1(m_VideoProcessor.Get(), frameFullRange ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020);

        if(m_VideoEnhancement->isVendorNVIDIA()){
            // VSR from Nvidia does not work yet on HDR content (Observation by March 28th, 2024)
            // https://en.wikipedia.org/wiki/Video_Super_Resolution#:~:text=The%20feature%20supports%20input%20resolutions,likely%20added%20in%20the%20future
            enableNvidiaVideoSuperResolution(false);
        } else if(m_VideoEnhancement->isVendorIntel()){
            // Enable VSR for Intel when the Host has HDR activated.
            enableIntelVideoSuperResolution();
        }
        if(m_AmfInitialized){
            // Disable sharpness when HDR is enable on client side because it generates white borders
            m_AmfUpScaler->Flush();
            m_AmfUpScaler->Terminate();
            m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_SHARPNESS, 2.00);
            m_AmfUpScaler->Init(m_AmfUpScalerSurfaceFormat, m_DecoderParams.width, m_DecoderParams.height);
        }
        break;

    default:
        m_VideoContext->VideoProcessorSetStreamColorSpace1(m_VideoProcessor.Get(), 0, frameFullRange ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 : DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);
        m_VideoContext->VideoProcessorSetOutputColorSpace1(m_VideoProcessor.Get(), frameFullRange ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 : DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);

        if(m_VideoEnhancement->isVendorNVIDIA()){
            // Always enable NVIDIA VSR for SDR content
            enableNvidiaVideoSuperResolution();
        } else if(m_VideoEnhancement->isVendorIntel()){
            // Disable VSR for Intel when the Host has HDR disactivated to avoid having a grey screen.
            enableIntelVideoSuperResolution(false);
        }
        if(m_AmfInitialized){
            // Enable Sharpness for Non-HDR source (host)
            m_AmfUpScaler->Flush();
            m_AmfUpScaler->Terminate();
            m_AmfUpScaler->SetProperty(AMF_HQ_SCALER_SHARPNESS, m_AmfUpScalerSharpness ? 0.30 : 2.00);
            m_AmfUpScaler->Init(m_AmfUpScalerSurfaceFormat, m_DecoderParams.width, m_DecoderParams.height);
        }
    }
}

void D3D11VARenderer::renderVideo(AVFrame* frame)
{
    // Bind video rendering vertex buffer
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, m_VideoVertexBuffer.GetAddressOf(), &stride, &offset);

    UINT srvIndex = 0;
    if (m_BindDecoderOutputTextures) {
        // Our indexing logic depends on a direct mapping into m_VideoTextureResourceViews
        // based on the texture index provided by FFmpeg.
        srvIndex = (uintptr_t)frame->data[1];
        SDL_assert(srvIndex < m_VideoTextureResourceViews.size());
        if (srvIndex >= m_VideoTextureResourceViews.size()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unexpected texture index: %u",
                         srvIndex);
            return;
        }

        // Ensure decoding operations have completed using a dummy fence.
        // This is not necessary on modern GPU drivers, but it is required
        // on some older Intel GPU drivers that don't properly synchronize
        // the video engine with 3D operations.
        if (m_UseFenceHack && m_FenceType != SupportedFenceType::None) {
            ComPtr<ID3D11Device5> device5;
            ComPtr<ID3D11DeviceContext4> deviceContext4;
            if (SUCCEEDED(m_Device.As(&device5)) && SUCCEEDED(m_DeviceContext.As(&deviceContext4))) {
                ComPtr<ID3D11Fence> fence;
                if (SUCCEEDED(device5->CreateFence(0,
                                                   m_FenceType == SupportedFenceType::Monitored ?
                                                       D3D11_FENCE_FLAG_NONE : D3D11_FENCE_FLAG_NON_MONITORED,
                                                   IID_PPV_ARGS(&fence)))) {
                    if (SUCCEEDED(deviceContext4->Signal(fence.Get(), 1))) {
                        deviceContext4->Wait(fence.Get(), 1);
                    }
                }
            }
        }
    }
    else if(m_AmfInitialized){
        // AMD (RDNA2+)

        // Copy this frame (minus alignment padding) into a temporary video texture
        m_DeviceContext->CopySubresourceRegion(m_AmfTexture.Get(), 0, 0, 0, 0, (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1], &m_SrcBox);
        m_AmfContext->CreateSurfaceFromDX11Native(m_AmfTexture.Get(), &m_AmfSurface, nullptr);

        // Up Scaling => To a higher resolution than the application window to give more surface to the VSR to generate details and thus picture clarity
        m_AmfUpScaler->SubmitInput(m_AmfSurface);
        m_AmfUpScaler->QueryOutput(&m_AmfData);
        m_AmfUpScaler->Flush();

        m_AmfData->QueryInterface(amf::AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurface));
        m_DeviceContext->CopyResource(m_VideoTexture.Get(), (ID3D11Texture2D*)m_AmfSurface->GetPlaneAt(0)->GetNative());
    } else if(m_VideoEnhancement->isVideoEnhancementEnabled() && !m_AmfInitialized){
        // NVIDIA RTX 2000+
        // Intel Arc+

        // Copy this frame (minus alignment padding) into a temporary video texture
        m_DeviceContext->CopySubresourceRegion(m_EnhancedTexture.Get(), 0, 0, 0, 0, (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1], &m_SrcBox);
        // Process operations on the output Texture
        m_VideoContext->VideoProcessorBlt(m_VideoProcessor.Get(), m_OutputView.Get(), 0, 1, &m_StreamData);
    } else {
        // No Enhancement processing

        // Copy this frame (minus alignment padding) into a temporary video texture
        m_DeviceContext->CopySubresourceRegion(m_VideoTexture.Get(), 0, 0, 0, 0, (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1], &m_SrcBox);
    }

    // Bind our CSC shader (and constant buffer, if required)
    bindColorConversion(frame);

    // Bind SRVs for this frame
    ID3D11ShaderResourceView* frameSrvs[] = { m_VideoTextureResourceViews[srvIndex][0].Get(), m_VideoTextureResourceViews[srvIndex][1].Get() };
    m_DeviceContext->PSSetShaderResources(0, 2, frameSrvs);

    // Process shaders on the output texture
    m_DeviceContext->DrawIndexed(6, 0, 0);

    // Unbind SRVs for this frame
    ID3D11ShaderResourceView* nullSrvs[2] = {};
    m_DeviceContext->PSSetShaderResources(0, 2, nullSrvs);
}

/**
 * \brief Add the Video Processor to the pipeline
 *
 * Creating a Video Processor add additional GPU video processing method like AI Upscaling
 *
 * \return bool Returns true if the Video processor is successfully created
 */
bool D3D11VARenderer::createVideoProcessor()
{
    HRESULT hr;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC content_desc;

    if(m_VideoProcessorEnumerator){
        m_VideoProcessorEnumerator.Reset();
    }
    if(m_VideoProcessor){
        m_VideoProcessor.Reset();
    }

    // Get video device
    hr = m_Device->QueryInterface(__uuidof(ID3D11VideoDevice),
                                  (void**)m_VideoDevice.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // Get video context
    hr = m_DeviceContext->QueryInterface(__uuidof(ID3D11VideoContext2),
                                         (void**)m_VideoContext.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    ZeroMemory(&content_desc, sizeof(content_desc));
    content_desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    content_desc.InputFrameRate.Numerator = m_DecoderParams.frameRate;
    content_desc.InputFrameRate.Denominator = 1;
    content_desc.InputWidth = m_DecoderParams.width;
    content_desc.InputHeight = m_DecoderParams.height;
    content_desc.OutputWidth = m_DisplayWidth;
    content_desc.OutputHeight = m_DisplayHeight;
    content_desc.OutputFrameRate.Numerator = m_DecoderParams.frameRate;
    content_desc.OutputFrameRate.Denominator = 1;
    content_desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    hr = m_VideoDevice->CreateVideoProcessorEnumerator(&content_desc, &m_VideoProcessorEnumerator);
    if (FAILED(hr))
        return false;

    hr = m_VideoDevice->CreateVideoProcessor(m_VideoProcessorEnumerator.Get(), 0,
                                             &m_VideoProcessor);
    if (FAILED(hr))
        return false;

    hr = m_VideoProcessorEnumerator->GetVideoProcessorCaps(&m_VideoProcessorCapabilities);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

/**
 * \brief Set the Video Processor to the pipeline
 *
 * Set proper Color space, filtering, and additional GPU video processing method like AI Upscaling
 *
 * \return bool Returns true if the Video processor is successfully setup
 */
bool D3D11VARenderer::initializeVideoProcessor()
{
    HRESULT hr;

    // INPUT setting
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc;

    ZeroMemory(&inputViewDesc, sizeof(inputViewDesc));
    inputViewDesc.FourCC = 0;
    inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputViewDesc.Texture2D.MipSlice = 0;
    inputViewDesc.Texture2D.ArraySlice = 0;

    hr = m_VideoDevice->CreateVideoProcessorInputView(
        m_EnhancedTexture.Get(),
        m_VideoProcessorEnumerator.Get(),
        &inputViewDesc,
        (ID3D11VideoProcessorInputView**)&m_InputView);
    if (FAILED(hr))
        return false;

    RECT inputRect = { 0 };
    inputRect.right = m_DisplayWidth;
    inputRect.bottom = m_DisplayHeight;
    m_VideoContext->VideoProcessorSetStreamDestRect(m_VideoProcessor.Get(), 0, true, &inputRect);

    m_VideoContext->VideoProcessorSetStreamFrameFormat(m_VideoProcessor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);

    // Initialize Color spaces, this will be adjusted once the first frame is received
    if(m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT){
        m_VideoContext->VideoProcessorSetStreamColorSpace1(m_VideoProcessor.Get(), 0, DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020);
    } else {
        m_VideoContext->VideoProcessorSetStreamColorSpace1(m_VideoProcessor.Get(), 0, DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);
    }


    // OUTPUT setting
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc;

    ZeroMemory(&outputViewDesc, sizeof(outputViewDesc));
    outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputViewDesc.Texture2D.MipSlice = 0;

    hr = m_VideoDevice->CreateVideoProcessorOutputView(
        m_VideoTexture.Get(),
        m_VideoProcessorEnumerator.Get(),
        &outputViewDesc,
        (ID3D11VideoProcessorOutputView**)&m_OutputView);
    if (FAILED(hr))
        return false;

    RECT targetRect = { 0 };
    targetRect.right = m_DisplayWidth;
    targetRect.bottom = m_DisplayHeight;
    m_VideoContext->VideoProcessorSetOutputTargetRect(m_VideoProcessor.Get(), true, &targetRect);

    m_VideoContext->VideoProcessorSetStreamOutputRate(m_VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, false, NULL);

    // Set Background color
    D3D11_VIDEO_COLOR bgColor;
    bgColor.RGBA = { 0, 0, 0, 1 }; // black color
    m_VideoContext->VideoProcessorSetOutputBackgroundColor(m_VideoProcessor.Get(), false, &bgColor);

    // Initialize Color spaces, this will be adjusted once the first frame is received
    if(m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT){
        m_VideoContext->VideoProcessorSetOutputColorSpace1(m_VideoProcessor.Get(), DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020);
    } else {
        m_VideoContext->VideoProcessorSetOutputColorSpace1(m_VideoProcessor.Get(), DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709);
    }

    // The section is a customization per vendor to slightly enhance (non-AI methods) the frame appearance.
    // It does work in addition to AI-enhancement for better result.
    if(m_VideoEnhancement->isVendorAMD()){
        // AMD does not have such filters
    } else if(m_VideoEnhancement->isVendorIntel()){
        // Reduce blocking artifacts
        if (m_VideoProcessorCapabilities.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION)
            m_VideoContext->VideoProcessorSetStreamFilter(m_VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION, true, 30); // (0 / 0 / 100)
        // Sharpen sligthly the picture to enhance details
        if (m_VideoProcessorCapabilities.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT)
            m_VideoContext->VideoProcessorSetStreamFilter(m_VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT, true, 20); // (0 / 0 / 100)
    } else if(m_VideoEnhancement->isVendorNVIDIA()){
        // Reduce blocking artifacts
        if (m_VideoProcessorCapabilities.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION)
            m_VideoContext->VideoProcessorSetStreamFilter(m_VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_FILTER_NOISE_REDUCTION, true, 30); // (0 / 0 / 100)
        // Sharpen sligthly the picture to enhance details
        if (m_VideoProcessorCapabilities.FilterCaps & D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT)
            m_VideoContext->VideoProcessorSetStreamFilter(m_VideoProcessor.Get(), 0, D3D11_VIDEO_PROCESSOR_FILTER_EDGE_ENHANCEMENT, true, 20); // (0 / 0 / 100)
    }

    ZeroMemory(&m_StreamData, sizeof(m_StreamData));
    m_StreamData.Enable = true;
    m_StreamData.OutputIndex = m_OutputIndex;
    m_StreamData.InputFrameOrField = 0;
    m_StreamData.PastFrames = 0;
    m_StreamData.FutureFrames = 0;
    m_StreamData.ppPastSurfaces = nullptr;
    m_StreamData.ppFutureSurfaces = nullptr;
    m_StreamData.pInputSurface = m_InputView.Get();
    m_StreamData.ppPastSurfacesRight = nullptr;
    m_StreamData.ppFutureSurfacesRight = nullptr;
    m_StreamData.pInputSurfaceRight = nullptr;

    return true;
}

// This function must NOT use any DXGI or ID3D11DeviceContext methods
// since it can be called on an arbitrary thread!
void D3D11VARenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    HRESULT hr;

    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    bool overlayEnabled = Session::get()->getOverlayManager().isOverlayEnabled(type);
    if (newSurface == nullptr && overlayEnabled) {
        // The overlay is enabled and there is no new surface. Leave the old texture alone.
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    ComPtr<ID3D11Texture2D> oldTexture = std::move(m_OverlayTextures[type]);
    ComPtr<ID3D11Buffer> oldVertexBuffer = std::move(m_OverlayVertexBuffers[type]);
    ComPtr<ID3D11ShaderResourceView> oldTextureResourceView = std::move(m_OverlayTextureResourceViews[type]);
    SDL_AtomicUnlock(&m_OverlayLock);

    // If the overlay is disabled, we're done
    if (!overlayEnabled) {
        SDL_FreeSurface(newSurface);
        return;
    }

    // Create a texture with our pixel data
    SDL_assert(!SDL_MUSTLOCK(newSurface));
    SDL_assert(newSurface->format->format == SDL_PIXELFORMAT_ARGB8888);

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = newSurface->w;
    texDesc.Height = newSurface->h;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA texData = {};
    texData.pSysMem = newSurface->pixels;
    texData.SysMemPitch = newSurface->pitch;

    ComPtr<ID3D11Texture2D> newTexture;
    hr = m_Device->CreateTexture2D(&texDesc, &texData, newTexture.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateTexture2D() failed: %x",
                     hr);
        return;
    }

    ComPtr<ID3D11ShaderResourceView> newTextureResourceView;
    hr = m_Device->CreateShaderResourceView((ID3D11Resource*)newTexture.Get(), nullptr, newTextureResourceView.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateShaderResourceView() failed: %x",
                     hr);
        return;
    }

    SDL_FRect renderRect = {};

    if (type == Overlay::OverlayStatusUpdate) {
        // Bottom Left
        renderRect.x = 0;
        renderRect.y = 0;
    }
    else if (type == Overlay::OverlayDebug) {
        // Top left
        renderRect.x = 0;
        renderRect.y = m_DisplayHeight - newSurface->h;
    }

    renderRect.w = newSurface->w;
    renderRect.h = newSurface->h;

    // Convert screen space to normalized device coordinates
    StreamUtils::screenSpaceToNormalizedDeviceCoords(&renderRect, m_DisplayWidth, m_DisplayHeight);

    // The surface is no longer required
    SDL_FreeSurface(newSurface);
    newSurface = nullptr;

    VERTEX verts[] =
        {
         {renderRect.x, renderRect.y, 0, 1},
         {renderRect.x, renderRect.y+renderRect.h, 0, 0},
         {renderRect.x+renderRect.w, renderRect.y, 1, 1},
         {renderRect.x+renderRect.w, renderRect.y+renderRect.h, 1, 0},
         };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(verts);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = sizeof(VERTEX);

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = verts;

    ComPtr<ID3D11Buffer> newVertexBuffer;
    hr = m_Device->CreateBuffer(&vbDesc, &vbData, newVertexBuffer.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateBuffer() failed: %x",
                     hr);
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    m_OverlayVertexBuffers[type] = std::move(newVertexBuffer);
    m_OverlayTextures[type] = std::move(newTexture);
    m_OverlayTextureResourceViews[type] = std::move(newTextureResourceView);
    SDL_AtomicUnlock(&m_OverlayLock);
}

bool D3D11VARenderer::checkDecoderSupport(IDXGIAdapter* adapter)
{
    HRESULT hr;

    DXGI_ADAPTER_DESC adapterDesc;
    hr = adapter->GetDesc(&adapterDesc);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIAdapter::GetDesc() failed: %x",
                     hr);
        return false;
    }

    if(m_VideoDevice == nullptr){
        createVideoProcessor();
    }

    // Check if the format is supported by this decoder
    BOOL supported;
    switch (m_DecoderParams.videoFormat)
    {
    case VIDEO_FORMAT_H264:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_H264_VLD_NOFGT, DXGI_FORMAT_NV12, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support H.264 decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support H.264 decoding to NV12 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_H264_HIGH8_444:
        // Unsupported by DXVA
        return false;

    case VIDEO_FORMAT_H265:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_HEVC_VLD_MAIN, DXGI_FORMAT_NV12, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC decoding to NV12 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_H265_MAIN10:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, DXGI_FORMAT_P010, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main10 decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main10 decoding to P010 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_H265_REXT8_444:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN_444, DXGI_FORMAT_AYUV, &supported)))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main 444 8-bit decoding via D3D11VA");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main 444 8-bit decoding to AYUV format");
            return false;
        }
        break;

    case VIDEO_FORMAT_H265_REXT10_444:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10_444, DXGI_FORMAT_Y410, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main 444 10-bit decoding via D3D11VA");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main 444 10-bit decoding to Y410 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_AV1_MAIN8:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0, DXGI_FORMAT_NV12, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 decoding to NV12 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_AV1_MAIN10:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0, DXGI_FORMAT_P010, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 Main 10-bit decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 Main 10-bit decoding to P010 format");
            return false;
        }
        break;

    case VIDEO_FORMAT_AV1_HIGH8_444:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_AV1_VLD_PROFILE1, DXGI_FORMAT_AYUV, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 High 444 8-bit decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 High 444 8-bit decoding to AYUV format");
            return false;
        }
        break;

    case VIDEO_FORMAT_AV1_HIGH10_444:
        if (FAILED(m_VideoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_AV1_VLD_PROFILE1, DXGI_FORMAT_Y410, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 High 444 10-bit decoding");
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support AV1 High 444 10-bit decoding to Y410 format");
            return false;
        }
        break;

    default:
        SDL_assert(false);
        return false;
    }

    if (DXUtil::isFormatHybridDecodedByHardware(m_DecoderParams.videoFormat, adapterDesc.VendorId, adapterDesc.DeviceId)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPU decoding for format %x is blocked due to hardware limitations",
                    m_DecoderParams.videoFormat);
        return false;
    }

    return true;
}

int D3D11VARenderer::getRendererAttributes()
{
    int attributes = 0;

    // This renderer supports HDR
    attributes |= RENDERER_ATTRIBUTE_HDR_SUPPORT;

    // This renderer requires frame pacing to synchronize with VBlank when we're in full-screen.
    // In windowed mode, we will render as fast we can and DWM will grab whatever is latest at the
    // time unless the user opts for pacing. We will use pacing in full-screen mode and normal DWM
    // sequencing in full-screen desktop mode to behave similarly to the DXVA2 renderer.
    if ((SDL_GetWindowFlags(m_DecoderParams.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
        attributes |= RENDERER_ATTRIBUTE_FORCE_PACING;
    }

    return attributes;
}

int D3D11VARenderer::getDecoderCapabilities()
{
    return CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC |
           CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
}

bool D3D11VARenderer::needsTestFrame()
{
    // We can usually determine when D3D11VA will work based on which decoder GUIDs are supported,
    // however there are some strange cases (Quadro P400 + Radeon HD 5570) where something goes
    // horribly wrong and D3D11VideoDevice::CreateVideoDecoder() fails inside FFmpeg. We need to
    // catch that case before we commit to using D3D11VA.
    return true;
}

IFFmpegRenderer::InitFailureReason D3D11VARenderer::getInitFailureReason()
{
    // In the specific case where we found at least one D3D11 hardware device but none of the
    // enumerated devices have support for the specified codec, tell the FFmpeg decoder not to
    // bother trying other hwaccels. We don't want to try loading D3D9 if the device doesn't
    // even have hardware support for the codec.
    //
    // NB: We use feature level 11.0 support as a gate here because we want to avoid returning
    // this failure reason in cases where we might have an extremely old GPU with support for
    // DXVA2 on D3D9 but not D3D11VA on D3D11. I'm unsure if any such drivers/hardware exists,
    // but better be safe than sorry.
    //
    // NB2: We're also assuming that no GPU exists which lacks any D3D11 driver but has drivers
    // for non-DX APIs like Vulkan. I believe this is a Windows Logo requirement so it should be
    // safe to assume.
    //
    // NB3: Sigh, there *are* GPUs drivers with greater codec support available via Vulkan than
    // D3D11VA even when both D3D11 and Vulkan APIs are supported. This is the case for HEVC RExt
    // profiles that were not supported by Microsoft until the Windows 11 24H2 SDK. Don't report
    // that hardware support is missing for YUV444 profiles since the Vulkan driver may support it.
    if (m_DevicesWithFL11Support != 0 && m_DevicesWithCodecSupport == 0 && !(m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_YUV444)) {
        return InitFailureReason::NoHardwareSupport;
    }
    else {
        return InitFailureReason::Unknown;
    }
}

void D3D11VARenderer::lockContext(void *lock_ctx)
{
    auto me = (D3D11VARenderer*)lock_ctx;

    SDL_LockMutex(me->m_ContextLock);
}

void D3D11VARenderer::unlockContext(void *lock_ctx)
{
    auto me = (D3D11VARenderer*)lock_ctx;

    SDL_UnlockMutex(me->m_ContextLock);
}

bool D3D11VARenderer::setupRenderingResources()
{
    HRESULT hr;

    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // We use a common vertex shader for all pixel shaders
    {
        QByteArray vertexShaderBytecode = Path::readDataFile("d3d11_vertex.fxc");

        ComPtr<ID3D11VertexShader> vertexShader;
        hr = m_Device->CreateVertexShader(vertexShaderBytecode.constData(), vertexShaderBytecode.length(), nullptr, vertexShader.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateVertexShader() failed: %x",
                         hr);
            return false;
        }

        const D3D11_INPUT_ELEMENT_DESC vertexDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        ComPtr<ID3D11InputLayout> inputLayout;
        hr = m_Device->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), vertexShaderBytecode.constData(), vertexShaderBytecode.length(), inputLayout.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->IASetInputLayout(inputLayout.Get());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateInputLayout() failed: %x",
                         hr);
            return false;
        }
    }

    {
        QByteArray overlayPixelShaderBytecode = Path::readDataFile("d3d11_overlay_pixel.fxc");

        hr = m_Device->CreatePixelShader(overlayPixelShaderBytecode.constData(), overlayPixelShaderBytecode.length(), nullptr, m_OverlayPixelShader.GetAddressOf());
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreatePixelShader() failed: %x",
                         hr);
            return false;
        }
    }

    for (int i = 0; i < PixelShaders::_COUNT; i++)
    {
        QByteArray videoPixelShaderBytecode = Path::readDataFile(k_VideoShaderNames[i]);
        hr = m_Device->CreatePixelShader(videoPixelShaderBytecode.constData(), videoPixelShaderBytecode.length(), nullptr, m_VideoPixelShaders[i].GetAddressOf());
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreatePixelShader() failed: %x",
                         hr);
            return false;
        }
    }

    // We use a common sampler for all pixel shaders
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        ComPtr<ID3D11SamplerState> sampler;
        hr = m_Device->CreateSamplerState(&samplerDesc,  sampler.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->PSSetSamplers(0, 1, sampler.GetAddressOf());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateSamplerState() failed: %x",
                         hr);
            return false;
        }
    }

    // Create our render target view
    {
        hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)m_BackBufferResource.GetAddressOf());
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "IDXGISwapChain::GetBuffer() failed: %x",
                         hr);
            return false;
        }

        hr = m_Device->CreateRenderTargetView(m_BackBufferResource.Get(), nullptr, m_RenderTargetView.GetAddressOf());
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateRenderTargetView() failed: %x",
                         hr);
            return false;
        }
    }

    // We use a common index buffer for all geometry
    {
        const int indexes[] = {0, 1, 2, 3, 2, 1};
        D3D11_BUFFER_DESC indexBufferDesc = {};
        indexBufferDesc.ByteWidth = sizeof(indexes);
        indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexBufferDesc.CPUAccessFlags = 0;
        indexBufferDesc.MiscFlags = 0;
        indexBufferDesc.StructureByteStride = sizeof(int);

        D3D11_SUBRESOURCE_DATA indexBufferData = {};
        indexBufferData.pSysMem = indexes;
        indexBufferData.SysMemPitch = sizeof(int);

        ComPtr<ID3D11Buffer> indexBuffer;
        hr = m_Device->CreateBuffer(&indexBufferDesc, &indexBufferData, indexBuffer.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBuffer() failed: %x",
                         hr);
            return false;
        }
    }

    // Create our fixed vertex buffer for video rendering
    {
        // Scale video to the window size while preserving aspect ratio
        SDL_Rect src, dst;
        if(m_AmfInitialized){
            // We use the full window for AMF as AMF keeps the picture ratio with black border around.
            dst.x = dst.y = 0;
            dst.w = m_DisplayWidth;
            dst.h = m_DisplayHeight;
        } else {
            src.x = src.y = 0;
            src.w = m_DecoderParams.width;
            src.h = m_DecoderParams.height;
            dst.x = dst.y = 0;
            dst.w = m_DisplayWidth;
            dst.h = m_DisplayHeight;
            StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
        }
        // Convert screen space to normalized device coordinates
        SDL_FRect renderRect;
        StreamUtils::screenSpaceToNormalizedDeviceCoords(&dst, &renderRect, m_DisplayWidth, m_DisplayHeight);

        // If we're binding the decoder output textures directly, don't sample from the alignment padding area
        SDL_assert(m_TextureAlignment != 0);
        float uMax = m_BindDecoderOutputTextures ? ((float)m_DecoderParams.width / FFALIGN(m_DecoderParams.width, m_TextureAlignment)) : 1.0f;
        float vMax = m_BindDecoderOutputTextures ? ((float)m_DecoderParams.height / FFALIGN(m_DecoderParams.height, m_TextureAlignment)) : 1.0f;

        VERTEX verts[] =
            {
             {renderRect.x, renderRect.y, 0, vMax},
             {renderRect.x, renderRect.y+renderRect.h, 0, 0},
             {renderRect.x+renderRect.w, renderRect.y, uMax, vMax},
             {renderRect.x+renderRect.w, renderRect.y+renderRect.h, uMax, 0},
             };

        D3D11_BUFFER_DESC vbDesc = {};
        vbDesc.ByteWidth = sizeof(verts);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = sizeof(VERTEX);

        D3D11_SUBRESOURCE_DATA vbData = {};
        vbData.pSysMem = verts;

        hr = m_Device->CreateBuffer(&vbDesc, &vbData, m_VideoVertexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBuffer() failed: %x",
                         hr);
            return false;
        }
    }

    // Create our fixed constant buffer to limit chroma texcoords and avoid sampling from alignment texels.
    {
        D3D11_BUFFER_DESC constDesc = {};
        constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
        constDesc.Usage = D3D11_USAGE_IMMUTABLE;
        constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constDesc.CPUAccessFlags = 0;
        constDesc.MiscFlags = 0;

        int textureWidth = m_BindDecoderOutputTextures ? FFALIGN(m_DecoderParams.width, m_TextureAlignment) : m_DecoderParams.width;
        int textureHeight = m_BindDecoderOutputTextures ? FFALIGN(m_DecoderParams.height, m_TextureAlignment) : m_DecoderParams.height;

        float chromaUVMax[3] = {};
        chromaUVMax[0] = m_DecoderParams.width != textureWidth ? ((float)(m_DecoderParams.width - 1) / textureWidth) : 1.0f;
        chromaUVMax[1] = m_DecoderParams.height != textureHeight ? ((float)(m_DecoderParams.height - 1) / textureHeight) : 1.0f;

        D3D11_SUBRESOURCE_DATA constData = {};
        constData.pSysMem = chromaUVMax;

        ComPtr<ID3D11Buffer> constantBuffer;
        HRESULT hr = m_Device->CreateBuffer(&constDesc, &constData, constantBuffer.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBuffer() failed: %x",
                         hr);
            return false;
        }
    }

    // Create our blend state
    {
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ComPtr<ID3D11BlendState> blendState;
        hr = m_Device->CreateBlendState(&blendDesc, blendState.GetAddressOf());
        if (SUCCEEDED(hr)) {
            m_DeviceContext->OMSetBlendState(blendState.Get(), nullptr, 0xffffffff);
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBlendState() failed: %x",
                         hr);
            return false;
        }
    }

    // Set a viewport that fills the window
    {
        D3D11_VIEWPORT viewport;

        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = m_DisplayWidth;
        viewport.Height = m_DisplayHeight;
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;

        m_DeviceContext->RSSetViewports(1, &viewport);
    }

    return true;
}

std::vector<DXGI_FORMAT> D3D11VARenderer::getVideoTextureSRVFormats()
{
    if (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_YUV444) {
        // YUV 4:4:4 formats don't use a second SRV
        return { (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT) ?
                    DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM };
    }
    else if (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT) {
        return { DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16G16_UNORM };
    }
    else {
        return { DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8G8_UNORM };
    }
}

/**
 * \brief Set the Texture used by AMD AMF
 *
 * Set a YUV texture to be processed by AMD AMF to upscale and denoise
 *
 * \return bool Returns true if the texture is created
 */
bool D3D11VARenderer::setupAmfTexture()
{
    // Texture description
    D3D11_TEXTURE2D_DESC texDesc = {};
    // Same size as the input Frame
    texDesc.Width = m_DecoderParams.width;
    texDesc.Height = m_DecoderParams.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = m_TextureFormat;
    texDesc.SampleDesc.Quality = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    HRESULT hr = m_Device->CreateTexture2D(&texDesc, nullptr, m_AmfTexture.GetAddressOf());
    if (FAILED(hr)) {
        // Handle error
        return false;
    }

    return true;
}

/**
 * \brief Set the Texture used by the Shaders
 *
 * Set a YUV texture to be processed by the shaders to convert to colorisatin to RGBA
 *
 * \return bool Returns true if the texture is created
 */
bool D3D11VARenderer::setupVideoTexture()
{
    SDL_assert(!m_BindDecoderOutputTextures);

    HRESULT hr;
    D3D11_TEXTURE2D_DESC texDesc = {};

    // Size of the output texture
    if(m_VideoEnhancement->isVideoEnhancementEnabled()){
        texDesc.Width = m_DisplayWidth;
        texDesc.Height = m_DisplayHeight;
    } else {
        texDesc.Width = m_DecoderParams.width;
        texDesc.Height = m_DecoderParams.height;
    }

    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT) ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Quality = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    // The flag D3D11_BIND_RENDER_TARGET is needed to enable the use of GPU enhancement
    if(m_VideoEnhancement->isVideoEnhancementEnabled()){
        texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    if(m_AmfInitialized){
        texDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    }

    hr = m_Device->CreateTexture2D(&texDesc, nullptr, m_VideoTexture.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateTexture2D() failed: %x",
                     hr);
        return false;
    }

    // Create SRVs for the texture
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    size_t srvIndex = 0;
    for (DXGI_FORMAT srvFormat : getVideoTextureSRVFormats()) {
        SDL_assert(srvIndex < m_VideoTextureResourceViews[0].size());

        srvDesc.Format = srvFormat;
        hr = m_Device->CreateShaderResourceView(m_VideoTexture.Get(), &srvDesc, &m_VideoTextureResourceViews[0][srvIndex]);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateShaderResourceView() failed: %x",
                         hr);
            return false;
        }

        srvIndex++;
    }

    return true;
}

bool D3D11VARenderer::setupTexturePoolViews(AVD3D11VAFramesContext* frameContext)
{
    SDL_assert(m_BindDecoderOutputTextures);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.ArraySize = 1;

    // Create luminance and chrominance SRVs for each texture in the pool
    for (size_t i = 0; i < m_VideoTextureResourceViews.size(); i++) {
        HRESULT hr;

        // Our rendering logic depends on the texture index working to map into our SRV array
        SDL_assert(i == (size_t)frameContext->texture_infos[i].index);

        srvDesc.Texture2DArray.FirstArraySlice = frameContext->texture_infos[i].index;

        size_t srvIndex = 0;
        for (DXGI_FORMAT srvFormat : getVideoTextureSRVFormats()) {
            SDL_assert(srvIndex < m_VideoTextureResourceViews[i].size());

            srvDesc.Format = srvFormat;
            hr = m_Device->CreateShaderResourceView(frameContext->texture_infos[i].texture,
                                                    &srvDesc,
                                                    &m_VideoTextureResourceViews[i][srvIndex]);
            if (FAILED(hr)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "ID3D11Device::CreateShaderResourceView() failed: %x",
                             hr);
                return false;
            }

            srvIndex++;
        }
    }

    return true;
}

/**
 * \brief Set the Texture used by the Video Processor
 *
 * Set a RGBA texture to be processed by the Video processor to upscale and denoise
 *
 * \return bool Returns true if the texture is created
 */
bool D3D11VARenderer::setupEnhancedTexture()
{
    HRESULT hr;
    D3D11_TEXTURE2D_DESC texDesc = {};

    // Size of the output texture
    if(m_AmfInitialized){
        texDesc.Width = m_OutputTexture.width;
        texDesc.Height = m_OutputTexture.height;
    } else {
        texDesc.Width = m_DecoderParams.width;
        texDesc.Height = m_DecoderParams.height;
    }
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT) ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
    texDesc.SampleDesc.Quality = 0;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = 0;
    if(m_AmfInitialized){
        texDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    }

    hr = m_Device->CreateTexture2D(&texDesc, nullptr, m_EnhancedTexture.GetAddressOf());
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateTexture2D() failed: %x",
                     hr);
        return false;
    }

    return true;
}
