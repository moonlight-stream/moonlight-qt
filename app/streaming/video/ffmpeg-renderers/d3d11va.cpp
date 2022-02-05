// For D3D11_DECODER_PROFILE values
#include <initguid.h>

#include "d3d11va.h"
#include "dxutil.h"
#include "path.h"

#include "streaming/streamutils.h"
#include "streaming/session.h"

#include <SDL_syswm.h>
#include <VersionHelpers.h>

#define SAFE_COM_RELEASE(x) if (x) { (x)->Release(); }

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

D3D11VARenderer::D3D11VARenderer()
    : m_Factory(nullptr),
      m_Device(nullptr),
      m_SwapChain(nullptr),
      m_DeviceContext(nullptr),
      m_RenderTargetView(nullptr),
      m_LastColorSpace(AVCOL_SPC_UNSPECIFIED),
      m_LastColorRange(AVCOL_RANGE_UNSPECIFIED),
      m_AllowTearing(false),
      m_FrameWaitableObject(nullptr),
      m_VideoPixelShader(nullptr),
      m_VideoVertexBuffer(nullptr),
      m_OverlayLock(0),
      m_OverlayPixelShader(nullptr),
      m_HwDeviceContext(nullptr),
      m_HwFramesContext(nullptr)
{
    RtlZeroMemory(m_OverlayVertexBuffers, sizeof(m_OverlayVertexBuffers));
    RtlZeroMemory(m_OverlayTextures, sizeof(m_OverlayTextures));
    RtlZeroMemory(m_OverlayTextureResourceViews, sizeof(m_OverlayTextureResourceViews));

    m_ContextLock = SDL_CreateMutex();
}

D3D11VARenderer::~D3D11VARenderer()
{
    SDL_DestroyMutex(m_ContextLock);

    SAFE_COM_RELEASE(m_VideoVertexBuffer);
    SAFE_COM_RELEASE(m_VideoPixelShader);

    for (int i = 0; i < ARRAYSIZE(m_OverlayVertexBuffers); i++) {
        SAFE_COM_RELEASE(m_OverlayVertexBuffers[i]);
    }

    for (int i = 0; i < ARRAYSIZE(m_OverlayTextureResourceViews); i++) {
        SAFE_COM_RELEASE(m_OverlayTextureResourceViews[i]);
    }

    for (int i = 0; i < ARRAYSIZE(m_OverlayTextures); i++) {
        SAFE_COM_RELEASE(m_OverlayTextures[i]);
    }

    SAFE_COM_RELEASE(m_OverlayPixelShader);

    SAFE_COM_RELEASE(m_RenderTargetView);

    if (m_FrameWaitableObject != nullptr) {
        CloseHandle(m_FrameWaitableObject);
    }

    if (m_SwapChain != nullptr && !m_Windowed) {
        // It's illegal to destroy a full-screen swapchain. Make sure we're in windowed mode.
        m_SwapChain->SetFullscreenState(FALSE, nullptr);
    }
    SAFE_COM_RELEASE(m_SwapChain);

    if (m_HwFramesContext != nullptr) {
        av_buffer_unref(&m_HwFramesContext);
    }

    if (m_HwDeviceContext != nullptr) {
        // This will release m_Device and m_DeviceContext too
        av_buffer_unref(&m_HwDeviceContext);
    }
    else {
        SAFE_COM_RELEASE(m_Device);
        SAFE_COM_RELEASE(m_DeviceContext);
    }

    SAFE_COM_RELEASE(m_Factory);
}

bool D3D11VARenderer::initialize(PDECODER_PARAMETERS params)
{
    int adapterIndex, outputIndex;
    HRESULT hr;

    m_DecoderParams = *params;

    // Use DXVA2 on anything older than Win10, so we don't have to handle a bunch
    // of legacy Win7/Win8 codepaths in here.
    if (!IsWindows10OrGreater()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D11VA renderer is only supported on Windows 10 or later.");
        return false;
    }

    if (!SDL_DXGIGetOutputInfo(SDL_GetWindowDisplayIndex(params->window),
                               &adapterIndex, &outputIndex)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_DXGIGetOutputInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    hr = CreateDXGIFactory(__uuidof(IDXGIFactory5), (void**)&m_Factory);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "CreateDXGIFactory() failed: %x",
                     hr);
        return false;
    }

    IDXGIAdapter* adapter;
    hr = m_Factory->EnumAdapters(adapterIndex, &adapter);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIFactory::EnumAdapters() failed: %x",
                     hr);
        return false;
    }

    hr = D3D11CreateDevice(adapter,
                           D3D_DRIVER_TYPE_UNKNOWN,
                           nullptr,
                           D3D11_CREATE_DEVICE_VIDEO_SUPPORT
                       #ifdef QT_DEBUG
                               | D3D11_CREATE_DEVICE_DEBUG
                       #endif
                           ,
                           nullptr,
                           0,
                           D3D11_SDK_VERSION,
                           &m_Device,
                           nullptr,
                           &m_DeviceContext);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D11CreateDevice() failed: %x",
                     hr);
        adapter->Release();
        return false;
    }

    if (!checkDecoderSupport(adapter)) {
        adapter->Release();
        return false;
    }

    adapter->Release();
    adapter = nullptr;

#if 0
    m_Windowed = (SDL_GetWindowFlags(params->window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != SDL_WINDOW_FULLSCREEN;
#else
    // Always use windowed or borderless windowed mode for now. SDL does mode-setting for us
    // in full-screen exclusive mode, so this actually works out okay.
    m_Windowed = true;
#endif

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc = {};

    if (m_Windowed) {
        // Use the current window size as the swapchain size
        SDL_GetWindowSize(params->window, (int*)&swapChainDesc.Width, (int*)&swapChainDesc.Height);
    }
    else {
        // Use the current display mode as the swapchain size
        SDL_DisplayMode sdlMode;
        if (SDL_GetWindowDisplayMode(params->window, &sdlMode) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_GetWindowDisplayMode() failed: %s",
                         SDL_GetError());
            return false;
        }

        swapChainDesc.Width = sdlMode.w;
        swapChainDesc.Height = sdlMode.h;

        // FIXME: The SDL referesh rate may not match the actual mode due to truncation
        fullScreenDesc.RefreshRate.Numerator = sdlMode.refresh_rate;
        fullScreenDesc.RefreshRate.Denominator = 1;

        fullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        fullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        fullScreenDesc.Windowed = FALSE;
    }

    m_DisplayWidth = swapChainDesc.Width;
    m_DisplayHeight = swapChainDesc.Height;

    if (params->videoFormat == VIDEO_FORMAT_H265_MAIN10) {
        swapChainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
    }
    else {
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    // Use DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING with flip mode for non-vsync case, if possible
    if (!params->enableVsync) {
        // DXGI_PRESENT_ALLOW_TEARING may only be used in windowed mode
        if (m_Windowed) {
            BOOL allowTearing = FALSE;
            hr = m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                &allowTearing,
                                                sizeof(allowTearing));
            if (SUCCEEDED(hr)) {
                // Use flip discard with allow tearing mode if possible.
                swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                m_AllowTearing = true;
            }
            else {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "GPU driver doesn't support DXGI_FEATURE_PRESENT_ALLOW_TEARING");

                // Without tearing support, we'll have to use regular discard mode to get tearing
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            }
        }
        else {
            // In full-screen exclusive mode, we'll have to use regular discard mode
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        }
    }
    else {
        // In V-sync mode, we can always use flip discard
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        // We'll use a waitable swapchain to ensure we get the lowest possible latency.
        // NB: We can only use this option in windowed mode.
        if (m_Windowed) {
            swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        }
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(params->window, &info);
    SDL_assert(info.subsystem == SDL_SYSWM_WINDOWS);

    IDXGISwapChain1* swapChain;
    hr = m_Factory->CreateSwapChainForHwnd(m_Device,
                                           info.info.win.window,
                                           &swapChainDesc,
                                           m_Windowed ? nullptr : &fullScreenDesc,
                                           nullptr,
                                           &swapChain);

    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIFactory::CreateSwapChainForHwnd() failed: %x",
                     hr);
        return false;
    }

    hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&m_SwapChain);
    swapChain->Release();

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

    if (!setupRenderingResources()) {
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

        // AVHWDeviceContext takes ownership of these objects
        d3d11vaDeviceContext->device = m_Device;
        d3d11vaDeviceContext->device_context = m_DeviceContext;

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

    {
        m_HwFramesContext = av_hwframe_ctx_alloc(m_HwDeviceContext);
        if (!m_HwFramesContext) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to allocate D3D11VA frame context");
            return false;
        }

        AVHWFramesContext* framesContext = (AVHWFramesContext*)m_HwFramesContext->data;

        // We require NV12 or P010 textures for our shader
        framesContext->format = AV_PIX_FMT_D3D11;
        framesContext->sw_format = params->videoFormat == VIDEO_FORMAT_H265_MAIN10 ?
                    AV_PIX_FMT_P010 : AV_PIX_FMT_NV12;

        // Surfaces must be 128 pixel aligned for HEVC and 16 pixel aligned for H.264
        framesContext->width = FFALIGN(params->width, (params->videoFormat & VIDEO_FORMAT_MASK_H265) ? 128 : 16);
        framesContext->height = FFALIGN(params->height, (params->videoFormat & VIDEO_FORMAT_MASK_H265) ? 128 : 16);

        // We can have up to 16 reference frames plus a working surface
        framesContext->initial_pool_size = 17;

        AVD3D11VAFramesContext* d3d11vaFramesContext = (AVD3D11VAFramesContext*)framesContext->hwctx;

        // We need to override the default D3D11VA bind flags to bind the textures as a shader resources
        d3d11vaFramesContext->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

        int err = av_hwframe_ctx_init(m_HwFramesContext);
        if (err < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to initialize D3D11VA frame context: %d",
                         err);
            return false;
        }
    }

    if (params->enableVsync && m_Windowed) {
        // We only want one buffered frame on our waitable swapchain
        hr = m_SwapChain->SetMaximumFrameLatency(1);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "IDXGISwapChain::SetMaximumFrameLatency() failed: %x",
                         hr);
            return false;
        }

        m_FrameWaitableObject = m_SwapChain->GetFrameLatencyWaitableObject();
        SDL_assert(m_FrameWaitableObject != nullptr);

        // Wait for the swap chain to be ready. This is required because we don't
        // we're waiting after presenting in the general case, not before.
        WaitForSingleObjectEx(m_FrameWaitableObject, 1000, FALSE);
    }
    else {
        IDXGIDevice1* dxgiDevice;

        hr = m_Device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&dxgiDevice);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::QueryInterface(IDXGIDevice1) failed: %x",
                         hr);
            return false;
        }

        // For the non-vsync case, we won't have a waitable swapchain,
        // so we must use IDXGIDevice1::SetMaximumFrameLatency() instead.
        hr = dxgiDevice->SetMaximumFrameLatency(1);

        dxgiDevice->Release();

        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "IDXGIDevice1::SetMaximumFrameLatency() failed: %x",
                         hr);
            return false;
        }
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

void D3D11VARenderer::setHdrMode(bool enabled)
{
    HRESULT hr;

    // According to MSDN, we need to lock the context even if we're just using DXGI functions
    // https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-intro
    lockContext(this);

    if (enabled) {
        DXGI_HDR_METADATA_HDR10 hdr10Metadata;

        hdr10Metadata.RedPrimary[0] = m_DecoderParams.hdrMetadata.displayPrimaries[0].x;
        hdr10Metadata.RedPrimary[1] = m_DecoderParams.hdrMetadata.displayPrimaries[0].y;
        hdr10Metadata.GreenPrimary[0] = m_DecoderParams.hdrMetadata.displayPrimaries[1].x;
        hdr10Metadata.GreenPrimary[1] = m_DecoderParams.hdrMetadata.displayPrimaries[1].y;
        hdr10Metadata.BluePrimary[0] = m_DecoderParams.hdrMetadata.displayPrimaries[2].x;
        hdr10Metadata.BluePrimary[1] = m_DecoderParams.hdrMetadata.displayPrimaries[2].y;
        hdr10Metadata.WhitePoint[0] = m_DecoderParams.hdrMetadata.whitePoint.x;
        hdr10Metadata.WhitePoint[1] = m_DecoderParams.hdrMetadata.whitePoint.y;
        hdr10Metadata.MaxMasteringLuminance = m_DecoderParams.hdrMetadata.maxDisplayMasteringLuminance;
        hdr10Metadata.MinMasteringLuminance = m_DecoderParams.hdrMetadata.minDisplayMasteringLuminance;
        hdr10Metadata.MaxContentLightLevel = m_DecoderParams.hdrMetadata.maxContentLightLevel;
        hdr10Metadata.MaxFrameAverageLightLevel = m_DecoderParams.hdrMetadata.maxFrameAverageLightLevel;

        hr = m_SwapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(hdr10Metadata), &hdr10Metadata);
        if (SUCCEEDED(hr)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Set display HDR mode: enabled");
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to enter HDR mode: %x",
                         hr);
        }

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

        hr = m_SwapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
        if (SUCCEEDED(hr)) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Set display HDR mode: disabled");
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to exit HDR mode: %x",
                         hr);
        }
    }

    unlockContext(this);
}

void D3D11VARenderer::renderFrame(AVFrame* frame)
{
    D3D11_VIEWPORT viewPort;

    if (frame == nullptr) {
        // End of stream - nothing to do for us
        return;
    }

    // Acquire the context lock for rendering to prevent concurrent
    // access from inside FFmpeg's decoding code
    lockContext(this);

    // Clear the back buffer
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clearColor);

    // Bind the back buffer. This needs to be done each time,
    // because the render target view will be unbound by Present().
    m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);

    viewPort.MinDepth = 0;
    viewPort.MaxDepth = 1;

    // Set the viewport to render the video with aspect ratio scaling
    SDL_Rect src, dst;
    src.x = src.y = 0;
    src.w = m_DecoderParams.width;
    src.h = m_DecoderParams.height;
    dst.x = dst.y = 0;
    dst.w = m_DisplayWidth;
    dst.h = m_DisplayHeight;
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
    viewPort.TopLeftX = dst.x;
    viewPort.TopLeftY = dst.y;
    viewPort.Width = dst.w;
    viewPort.Height = dst.h;
    m_DeviceContext->RSSetViewports(1, &viewPort);

    // Render our video frame with the aspect-ratio adjusted viewport
    renderVideo(frame);

    // Set the viewport to render overlays at the full window size
    viewPort.TopLeftX = viewPort.TopLeftY = 0;
    viewPort.Width = m_DisplayWidth;
    viewPort.Height = m_DisplayHeight;
    m_DeviceContext->RSSetViewports(1, &viewPort);

    // Render overlays on top of the video stream
    for (int i = 0; i < Overlay::OverlayMax; i++) {
        renderOverlay((Overlay::OverlayType)i);
    }

    UINT flags;
    UINT syncInterval;

    if (m_AllowTearing) {
        SDL_assert(!m_DecoderParams.enableVsync);
        SDL_assert(m_Windowed);

        // If tearing is allowed, use DXGI_PRESENT_ALLOW_TEARING with syncInterval 0.
        // It is not valid to use any other syncInterval values in tearing mode.
        syncInterval = 0;
        flags = DXGI_PRESENT_ALLOW_TEARING;
    }
    else if (!m_DecoderParams.enableVsync) {
        // In any other non-vsync mode, just render with syncInterval 0.
        // We'll probably have a non-flip swapchain here.
        syncInterval = 0;
        flags = 0;
    }
    else if (m_Windowed) {
        SDL_assert(m_DecoderParams.enableVsync);

        // In windowed mode, we'll have a waitable swapchain, so we can
        // use syncInterval 0 and the wait will sync us with VBlank.
        syncInterval = 0;
        flags = 0;
    }
    else {
        SDL_assert(!m_Windowed);
        SDL_assert(m_DecoderParams.enableVsync);
        SDL_assert(m_FrameWaitableObject == nullptr);

        // In full-screen exclusive mode, we won't have waitable swapchain.
        // We'll use syncInterval 1 to synchronize with VBlank and pass
        // DXGI_PRESENT_DO_NOT_WAIT for our flags to avoid blocking any
        // concurrent decoding operations in flight.
        syncInterval = 1;
        flags = DXGI_PRESENT_DO_NOT_WAIT;
    }

    HRESULT hr;
    do {
        // Present according to the decoder parameters
        hr = m_SwapChain->Present(syncInterval, flags);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING) {
            // Unlock the context while we wait to try again
            unlockContext(this);
            SDL_Delay(1);
            lockContext(this);
        }
    } while (hr == DXGI_ERROR_WAS_STILL_DRAWING);

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

    if (m_FrameWaitableObject != nullptr) {
        SDL_assert(m_Windowed);
        SDL_assert(m_DecoderParams.enableVsync);

        // Wait for the pipeline to be ready for the next frame in V-Sync mode.
        //
        // MSDN advises us to wait *before* doing any rendering operations,
        // however that assumes the a typical game which will latch inputs,
        // run the engine, draw, etc. after WaitForSingleObjectEx(). In our case,
        // we actually want wait *after* our rendering operations, because our AVFrame
        // is already set in stone by the time we enter this function. Waiting after
        // presenting allows a more recent frame to be received before renderFrame()
        // is called again.
        WaitForSingleObjectEx(m_FrameWaitableObject, 1000, FALSE);
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

    ID3D11Texture2D* overlayTexture = m_OverlayTextures[type];
    ID3D11Buffer* overlayVertexBuffer = m_OverlayVertexBuffers[type];
    ID3D11ShaderResourceView* overlayTextureResourceView = m_OverlayTextureResourceViews[type];

    if (overlayTexture == nullptr) {
        SDL_AtomicUnlock(&m_OverlayLock);
        return;
    }

    // Reference these objects so they don't immediately go away if the
    // overlay update thread tries to release them.
    SDL_assert(overlayVertexBuffer != nullptr);
    overlayTexture->AddRef();
    overlayVertexBuffer->AddRef();
    overlayTextureResourceView->AddRef();

    SDL_AtomicUnlock(&m_OverlayLock);

    // Bind vertex buffer
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &overlayVertexBuffer, &stride, &offset);

    // Bind pixel shader and resources
    m_DeviceContext->PSSetShader(m_OverlayPixelShader, nullptr, 0);
    m_DeviceContext->PSSetShaderResources(0, 1, &overlayTextureResourceView);

    // Draw the overlay
    m_DeviceContext->DrawIndexed(6, 0, 0);

    overlayTextureResourceView->Release();
    overlayTexture->Release();
    overlayVertexBuffer->Release();
}

void D3D11VARenderer::updateColorConversionConstants(AVFrame* frame)
{
    // If nothing has changed since last frame, we're done
    if (frame->colorspace == m_LastColorSpace && frame->color_range == m_LastColorRange) {
        return;
    }

    D3D11_BUFFER_DESC constDesc = {};
    constDesc.ByteWidth = sizeof(CSC_CONST_BUF);
    constDesc.Usage = D3D11_USAGE_IMMUTABLE;
    constDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constDesc.CPUAccessFlags = 0;
    constDesc.MiscFlags = 0;

    // This handles the case where the color range is unknown,
    // so that we use Limited color range which is the default
    // behavior for Moonlight.
    CSC_CONST_BUF constBuf = {};
    bool fullRange = (frame->color_range == AVCOL_RANGE_JPEG);
    const float* rawCscMatrix;
    switch (frame->colorspace) {
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
        rawCscMatrix = fullRange ? k_CscMatrix_Bt601Full : k_CscMatrix_Bt601Lim;
        break;
    case AVCOL_SPC_BT709:
        rawCscMatrix = fullRange ? k_CscMatrix_Bt709Full : k_CscMatrix_Bt709Lim;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
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

    ID3D11Buffer* constantBuffer;
    HRESULT hr = m_Device->CreateBuffer(&constDesc, &constData, &constantBuffer);
    if (SUCCEEDED(hr)) {
        m_DeviceContext->PSSetConstantBuffers(0, 1, &constantBuffer);
        constantBuffer->Release();
    }
    else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateBuffer() failed: %x",
                     hr);
        return;
    }

    m_LastColorSpace = frame->colorspace;
    m_LastColorRange = frame->color_range;
}

void D3D11VARenderer::renderVideo(AVFrame* frame)
{
    HRESULT hr;

    // Update our CSC constants if the colorspace has changed
    updateColorConversionConstants(frame);

    // Bind video rendering vertex buffer
    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, &m_VideoVertexBuffer, &stride, &offset);

    // Create shader resource views for the video texture
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = (uintptr_t)frame->data[1];
    srvDesc.Texture2DArray.ArraySize = 1;

    // Bind the luminance plane
    ID3D11ShaderResourceView* luminanceTextureView;
    srvDesc.Format = m_DecoderParams.videoFormat == VIDEO_FORMAT_H265_MAIN10 ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    hr = m_Device->CreateShaderResourceView((ID3D11Resource*)frame->data[0], &srvDesc, &luminanceTextureView);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateShaderResourceView() failed: %x",
                     hr);
        return;
    }
    m_DeviceContext->PSSetShaderResources(0, 1, &luminanceTextureView);
    luminanceTextureView->Release();

    // Bind the chrominance plane
    ID3D11ShaderResourceView* chrominanceTextureView;
    srvDesc.Format = m_DecoderParams.videoFormat == VIDEO_FORMAT_H265_MAIN10 ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    hr = m_Device->CreateShaderResourceView((ID3D11Resource*)frame->data[0], &srvDesc, &chrominanceTextureView);
    if (FAILED(hr)) {
        luminanceTextureView->Release();
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateShaderResourceView() failed: %x",
                     hr);
        return;
    }
    m_DeviceContext->PSSetShaderResources(1, 1, &chrominanceTextureView);
    chrominanceTextureView->Release();

    // Bind video pixel shader
    m_DeviceContext->PSSetShader(m_VideoPixelShader, nullptr, 0);

    // Draw the video
    m_DeviceContext->DrawIndexed(6, 0, 0);
}

// This function must NOT use any DXGI or ID3D11DeviceContext methods
// since it can be called on an arbitrary thread!
void D3D11VARenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    HRESULT hr;

    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    if (newSurface == nullptr && Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        // The overlay is enabled and there is no new surface. Leave the old texture alone.
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    ID3D11Texture2D* oldTexture = m_OverlayTextures[type];
    m_OverlayTextures[type] = nullptr;

    ID3D11Buffer* oldVertexBuffer = m_OverlayVertexBuffers[type];
    m_OverlayVertexBuffers[type] = nullptr;

    ID3D11ShaderResourceView* oldTextureResourceView = m_OverlayTextureResourceViews[type];
    m_OverlayTextureResourceViews[type] = nullptr;
    SDL_AtomicUnlock(&m_OverlayLock);

    SAFE_COM_RELEASE(oldTextureResourceView);
    SAFE_COM_RELEASE(oldTexture);
    SAFE_COM_RELEASE(oldVertexBuffer);

    // If the overlay is disabled, we're done
    if (!Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        SDL_FreeSurface(newSurface);
        return;
    }

    // Create a texture with our pixel data
    SDL_assert(!SDL_MUSTLOCK(newSurface));

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

    ID3D11Texture2D* newTexture;
    hr = m_Device->CreateTexture2D(&texDesc, &texData, &newTexture);
    if (FAILED(hr)) {
        SDL_FreeSurface(newSurface);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateTexture2D() failed: %x",
                     hr);
        return;
    }

    ID3D11ShaderResourceView* newTextureResourceView = nullptr;
    hr = m_Device->CreateShaderResourceView((ID3D11Resource*)newTexture, nullptr, &newTextureResourceView);
    if (FAILED(hr)) {
        SAFE_COM_RELEASE(newTexture);
        SDL_FreeSurface(newSurface);
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
    renderRect.x /= m_DisplayWidth / 2;
    renderRect.w /= m_DisplayWidth / 2;
    renderRect.y /= m_DisplayHeight / 2;
    renderRect.h /= m_DisplayHeight / 2;
    renderRect.x -= 1.0f;
    renderRect.y -= 1.0f;

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

    ID3D11Buffer* newVertexBuffer;
    hr = m_Device->CreateBuffer(&vbDesc, &vbData, &newVertexBuffer);
    if (FAILED(hr)) {
        SAFE_COM_RELEASE(newTextureResourceView);
        SAFE_COM_RELEASE(newTexture);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::CreateBuffer() failed: %x",
                     hr);
        return;
    }

    SDL_AtomicLock(&m_OverlayLock);
    m_OverlayVertexBuffers[type] = newVertexBuffer;
    m_OverlayTextures[type] = newTexture;
    m_OverlayTextureResourceViews[type] = newTextureResourceView;
    SDL_AtomicUnlock(&m_OverlayLock);
}

bool D3D11VARenderer::checkDecoderSupport(IDXGIAdapter* adapter)
{
    HRESULT hr;
    ID3D11VideoDevice* videoDevice;

    // Derive a ID3D11VideoDevice from our ID3D11Device.
    hr = m_Device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&videoDevice);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ID3D11Device::QueryInterface(ID3D11VideoDevice) failed: %x",
                     hr);
        return false;
    }

    // Check if the format is supported by this decoder
    BOOL supported;
    switch (m_DecoderParams.videoFormat)
    {
    case VIDEO_FORMAT_H264:
        if (FAILED(videoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_H264_VLD_NOFGT, DXGI_FORMAT_NV12, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support H.264 decoding");
            videoDevice->Release();
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support H.264 decoding to NV12 format");
            videoDevice->Release();
            return false;
        }
        break;

    case VIDEO_FORMAT_H265:
        if (FAILED(videoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_HEVC_VLD_MAIN, DXGI_FORMAT_NV12, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC decoding");
            videoDevice->Release();
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC decoding to NV12 format");
            videoDevice->Release();
            return false;
        }
        break;

    case VIDEO_FORMAT_H265_MAIN10:
        if (FAILED(videoDevice->CheckVideoDecoderFormat(&D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10, DXGI_FORMAT_P010, &supported))) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main10 decoding");
            videoDevice->Release();
            return false;
        }
        else if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GPU doesn't support HEVC Main10 decoding to P010 format");
            videoDevice->Release();
            return false;
        }
        break;

    default:
        SDL_assert(false);
        videoDevice->Release();
        return false;
    }

    videoDevice->Release();

    DXGI_ADAPTER_DESC adapterDesc;
    hr = adapter->GetDesc(&adapterDesc);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "IDXGIAdapter::GetDesc() failed: %x",
                     hr);
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
    // This renderer supports HDR
    return RENDERER_ATTRIBUTE_HDR_SUPPORT;
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

        ID3D11VertexShader* vertexShader;
        hr = m_Device->CreateVertexShader(vertexShaderBytecode.constData(), vertexShaderBytecode.length(), nullptr, &vertexShader);
        if (SUCCEEDED(hr)) {
            m_DeviceContext->VSSetShader(vertexShader, nullptr, 0);
            vertexShader->Release();
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
        ID3D11InputLayout* inputLayout;
        hr = m_Device->CreateInputLayout(vertexDesc, ARRAYSIZE(vertexDesc), vertexShaderBytecode.constData(), vertexShaderBytecode.length(), &inputLayout);
        if (SUCCEEDED(hr)) {
            m_DeviceContext->IASetInputLayout(inputLayout);
            inputLayout->Release();
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

        hr = m_Device->CreatePixelShader(overlayPixelShaderBytecode.constData(), overlayPixelShaderBytecode.length(), nullptr, &m_OverlayPixelShader);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreatePixelShader() failed: %x",
                         hr);
            return false;
        }
    }

    {
        QByteArray videoPixelShaderBytecode = Path::readDataFile("d3d11_video_pixel.fxc");

        hr = m_Device->CreatePixelShader(videoPixelShaderBytecode.constData(), videoPixelShaderBytecode.length(), nullptr, &m_VideoPixelShader);
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

        ID3D11SamplerState* sampler;
        hr = m_Device->CreateSamplerState(&samplerDesc,  &sampler);
        if (SUCCEEDED(hr)) {
            m_DeviceContext->PSSetSamplers(0, 1, &sampler);
            sampler->Release();
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
        ID3D11Resource* backBufferResource;
        hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&backBufferResource);
        if (FAILED(hr)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "IDXGISwapChain::GetBuffer() failed: %x",
                         hr);
            return false;
        }

        hr = m_Device->CreateRenderTargetView(backBufferResource, nullptr, &m_RenderTargetView);
        backBufferResource->Release();
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

        ID3D11Buffer* indexBuffer;
        hr = m_Device->CreateBuffer(&indexBufferDesc, &indexBufferData, &indexBuffer);
        if (SUCCEEDED(hr)) {
            m_DeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            indexBuffer->Release();
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
        VERTEX verts[] =
        {
            {-1, -1, 0, 1},
            {-1,  1, 0, 0},
            { 1, -1, 1, 1},
            { 1,  1, 1, 0},
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

        hr = m_Device->CreateBuffer(&vbDesc, &vbData, &m_VideoVertexBuffer);
        if (FAILED(hr)) {
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

        ID3D11BlendState* blendState;
        hr = m_Device->CreateBlendState(&blendDesc, &blendState);
        if (SUCCEEDED(hr)) {
            m_DeviceContext->OMSetBlendState(blendState, nullptr, 0xffffffff);
            blendState->Release();
        }
        else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D11Device::CreateBlendState() failed: %x",
                         hr);
            return false;
        }
    }

    return true;
}
