#include "d3d12va.h"

// Standard DXVA GUIDs for HEVC RExt profiles (redefined for compatibility with pre-24H2 SDKs)
DEFINE_GUID(k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN_444,   0x4008018f, 0xf537, 0x4b36, 0x98, 0xcf, 0x61, 0xaf, 0x8a, 0x2c, 0x1a, 0x33);
DEFINE_GUID(k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10_444, 0x0dabeffa, 0x4458, 0x4602, 0xbc, 0x03, 0x07, 0x95, 0x65, 0x9d, 0x61, 0x7c);

bool D3D12VARenderer::m_IsFrameHDR = false;
bool D3D12VARenderer::m_IsClientHDR = false;

/**
 * \brief Constructor
 */
D3D12VARenderer::D3D12VARenderer(int decoderSelectionPass):
    IFFmpegRenderer(RendererType::D3D12VA),
    m_SkipFrame(false),
    m_VideoEnhancement(&VideoEnhancement::getInstance()),
    m_Preferences(StreamingPreferences::get()),
    m_OverlayLock(0),
    m_HwDeviceContext(nullptr),
    m_AmfInitialized(false),
    m_IntelInitialized(false),
    m_NvidiaInitialized(false),
    m_AmfContext(nullptr),
    m_AmfData(nullptr),
    m_AmfUpscalerYUV(nullptr),
    m_AmfVideoConverter(nullptr),
    m_AmfUpscalerRGB(nullptr),
    m_AmfVideoConverterUpscaled(nullptr),
    m_AmfSurfaceYUV(nullptr),
    m_AmfSurfaceRGB(nullptr),
    m_AmfSurfaceUpscaledYUV(nullptr),
    m_AmfSurfaceUpscaledRGB(nullptr),
    m_VSRFeature(nullptr),
    m_TrueHDRFeature(nullptr),
    m_VppSession(nullptr)
{
    m_ContextLock = SDL_CreateMutex();
    
    // Give a High CPU priority to this thread
    DwmEnableMMCSS(TRUE);
    
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []()
    {
        // When the application leave, make sure we reset the Display setting
        ChangeDisplaySettingsW(nullptr, 0);
    });
}

/**
 * \brief Destructor
 */
D3D12VARenderer::~D3D12VARenderer()
{
    DwmEnableMMCSS(FALSE);
    
    // Reset the Display setting
    ChangeDisplaySettingsW(nullptr, 0);
    
    SDL_DestroyMutex(m_ContextLock);

    // Wait for the thread to finish properly
    m_cancelHDRUpdate = true;
    m_HDRUpdateFuture.waitForFinished();
    waitForVideoProcess(true);
    waitForGraphics(true);
    waitForOverlay(true);

    if (m_FenceVideoProcessEvent) CloseHandle(m_FenceVideoProcessEvent);
    if (m_FenceGraphicsEvent) CloseHandle(m_FenceGraphicsEvent);
    if (m_FenceOverlayEvent) CloseHandle(m_FenceOverlayEvent);
    if (m_FenceAMFEvent) CloseHandle(m_FenceAMFEvent);

    // Textures
    m_D3D11FrameTexture.Reset();
    m_FrameTexture.Reset();
    m_RGBTexture.Reset();
    m_RGBTextureUpscaled.Reset();
    m_YUVTextureUpscaled.Reset();
    m_OutputTexture.Reset();
    m_OutputTexturePrevious.Reset();
    for (auto& item : m_OverlayTextures) {
        item.Reset();
    }
    for (auto& item : m_BackBuffers) {
        item.Reset();
    }

    // Clean Shaders
    m_ShaderConverter.reset();
    m_ShaderUpscaler.reset();

    m_OverlayPSO.Reset();
    m_OverlayRootSignature.Reset();
    m_OverlaySrvHeap.Reset();
    m_RtvHeap.Reset();

    m_SwapChain.Reset();

    m_VideoProcessCommandAllocator.Reset();
    m_VideoProcessCommandList.Reset();
    m_VideoProcessCommandQueue.Reset();

    m_GraphicsCommandAllocator.Reset();
    m_GraphicsCommandList.Reset();
    m_GraphicsCommandQueue.Reset();

    m_OverlayCommandAllocator.Reset();
    m_OverlayCommandList.Reset();
    m_OverlayCommandQueue.Reset();

    m_PictureCommandAllocator.Reset();
    m_PictureCommandList.Reset();
    m_PictureCommandQueue.Reset();

    m_FenceVideoProcess.Reset();
    m_FenceGraphics.Reset();
    m_FenceOverlay.Reset();
    m_FenceAMF.Reset();

    m_VideoProcessorConvert.Reset();
    m_VideoProcessorUpscaler.Reset();
    m_VideoProcessorUpscalerConvert.Reset();
    m_VideoDevice.Reset();

    if (m_D3D11DeviceContext) {
        m_D3D11DeviceContext->Flush();
        m_D3D11DeviceContext->ClearState();
    }

    if (m_HwDeviceContext) {
        av_buffer_unref(&m_HwDeviceContext);
        m_HwDeviceContext = nullptr;
    }
    
    // Nvidia VSR and TrueHDR
    NVSDK_NGX_D3D12_ReleaseFeature(m_VSRFeature);
    m_VSRFeature = nullptr;
    NVSDK_NGX_D3D12_ReleaseFeature(m_TrueHDRFeature);
    m_TrueHDRFeature = nullptr;
    NVSDK_NGX_D3D12_DestroyParameters(m_VSRngxParameters);
    NVSDK_NGX_D3D12_DestroyParameters(m_TrueHDRngxParameters);
    
    // Intel VPL
    if(m_VppSession){
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            mfxSyncPoint syncp = nullptr;
            
            mfxStatus sts = MFXVideoVPP_RunFrameVPPAsync(
                m_VppSession,
                nullptr,   // NULL input = drain
                nullptr,
                nullptr,
                &syncp
                );
            
            if (sts == MFX_ERR_NONE && syncp) {
                MFXVideoCORE_SyncOperation(m_VppSession, syncp, INFINITE);
                continue;
            }
            
            if (sts == MFX_ERR_MORE_DATA)
                break;
            
            if (sts < 0)
                break;
        }
        MFXVideoVPP_Close(m_VppSession);
        MFXClose(m_VppSession);
        m_VppSession = nullptr;
    }
    
    for (auto& item : m_VppSurfaceIn) {
        item.surface = nullptr;
    }
    
    for (auto& item : m_VppSurfaceOut) {
        item.surface = nullptr;
    }

    m_D3D11VideoDevice.Reset();
    m_D3D11DeviceContext.Reset();
    m_D3D11Device.Reset();
    
    m_Device.Reset();
    m_Adapter.Reset();
    m_Factory.Reset();

#ifdef QT_DEBUG
    if(m_DebugVerbose){
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
        }
    }
#endif
}

/**
 * \brief Verify the result and display error logs
 *
 * Verify if the operation succeed
 * Display error log in case of failed
 *
 * \param HRESULT hr Result of the operation
 * \param const char* operation Additional message
 * \return bool Return true if succeed
 */
bool D3D12VARenderer::verifyHResult(HRESULT hr, const char* operation)
{
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "%s failed with HRESULT: 0x%08X", operation, hr);

        // Convert HRESULT in a readable string
        char errorMsg[256];
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       errorMsg, sizeof(errorMsg), nullptr);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error message: %s", errorMsg);

        if (m_Device) {
            // If debug layer available, print messages from ID3D12InfoQueue
            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
                UINT64 num = infoQueue->GetNumStoredMessages();
                for (UINT64 i = 0; i < num; ++i) {
                    SIZE_T msgSize = 0;
                    infoQueue->GetMessage(i, nullptr, &msgSize);
                    std::vector<char> buf(msgSize);
                    D3D12_MESSAGE* msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
                    infoQueue->GetMessage(i, msg, &msgSize);
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "D3D12Msg %u: %s", (unsigned)i, msg->pDescription);
                }
                infoQueue->ClearStoredMessages();
            }

            HRESULT removedHr = m_Device->GetDeviceRemovedReason();
            if (removedHr != S_OK) {
                char errorMsg[256];
                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM,
                    nullptr,
                    removedHr,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errorMsg,
                    sizeof(errorMsg),
                    nullptr
                    );

                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Device removed! HRESULT: 0x%08X, Message: %s",
                             removedHr, errorMsg);
            }
        }

        return false;
    }
    return true;
}

/**
 * \brief Check client display HDR
 *
 * Check if the client display has HDR enabled
 *
 * \return bool Returns true if success
 */
bool D3D12VARenderer::getDisplayHDRStatus()
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(m_DecoderParams.window, &info);

    POINT windowPoint = {};
    GetClientRect(info.info.win.window, (LPRECT)&windowPoint);
    ClientToScreen(info.info.win.window, &windowPoint);
    
    ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
        return false;

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0;
         SUCCEEDED(factory->EnumAdapters1(adapterIndex, &adapter));
         ++adapterIndex)
    {
        ComPtr<IDXGIOutput> output;
        for (UINT outputIndex = 0;
             SUCCEEDED(adapter->EnumOutputs(outputIndex, &output));
             ++outputIndex)
        {
            ComPtr<IDXGIOutput6> output6;
            if (FAILED(output.As(&output6)))
                continue;

            DXGI_OUTPUT_DESC1 desc = {};
            if (FAILED(output6->GetDesc1(&desc)))
                continue;
            
            m_MaxLuminance = desc.MaxLuminance;

            RECT desktopCoordinates = desc.DesktopCoordinates;
            if (PtInRect(&desktopCoordinates, windowPoint))
            {
                // Returns True if HDR is on
                return desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
                       desc.ColorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020 ||
                       desc.ColorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020 ||
                       desc.ColorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020 ||
                       desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020;
            }
        }
    }
    return false;
}

/**
 * \brief Update the value of HDR status
 *
 * Update the value of HDR enabler asynchronously to avoid blocking the main renderer.
 * This is a Multithread method as it is CPU consuming to not freeze the application.
 *
 * \params bool isFrameHDR
 * \return void
 */
void D3D12VARenderer::updateDisplayHDRStatusAsync(bool isFrameHDR)
{    
    // Swapchain ColorSpace can be changed in live
    if (isFrameHDR != D3D12VARenderer::m_IsFrameHDR) {
        D3D12VARenderer::m_IsFrameHDR = isFrameHDR;
        // Updating the swapChain shall be enough as it auto correct the output,
        // but we still need to reinitialise the renderer to make the whole chain aligned
        m_RGBColorSpace = (D3D12VARenderer::m_IsFrameHDR || m_VendorHDRenabled) ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709; 
        m_SwapChain->SetColorSpace1(m_RGBColorSpace);
        m_ReloadRenderer = true;
    }
    
    // Check the Client HDR status every 3s
    m_CheckHDRCount++;
    if (m_CheckHDRCount < 3 * m_DecoderParams.frameRate) {
        return;
    }

    // Skip if update is already running or paused
    if (m_PauseHDRUpdate || m_HDRUpdateFuture.isRunning())
        return;

    m_CheckHDRCount = 0;
    m_PauseHDRUpdate = true;

    // Create a safe Qt pointer that tracks object lifetime (QObject-aware)
    QPointer<D3D12VARenderer> self(this);
    m_HDRUpdateFuture = QtConcurrent::run([self]() {
        
        // The thread is all about this line, to not block the rendering thread
        // as it takes some time to get the information
        bool hdrEnabled = self->getDisplayHDRStatus();

        // Safely modify QObject from the main thread using queued signal
        QMetaObject::invokeMethod(self, [self, hdrEnabled]() {
            
            if (!self) return;
            
            // Check if the display HDR setting changed for Host and Client
            if (hdrEnabled != D3D12VARenderer::m_IsClientHDR || self->m_ReloadRenderer) {
                D3D12VARenderer::m_IsClientHDR = hdrEnabled;
                
                // Reload the Renderer to set properly Textures format, Color spaces, etc.
                SDL_Event event;
                event.type = SDL_RENDER_TARGETS_RESET;
                SDL_PushEvent(&event);
            }
            // Reset the pause flag (executed in the main GUI thread)
            self->m_PauseHDRUpdate = false;
        }, Qt::QueuedConnection);
    });
}

/**
 * \brief Set the most appropriate Enhancement method according to the GPU
 *
 * Based on multiple performance (latency) and picture quality tests on divers GPU/iGPU,
 * the method will setup the most apprioriate setting for enhanced rendering.
 * NOTE: Any change to this method needs to be widely tested as each GPU acts differently, regression could be observed.
 * Documentation: In comments of the following PR, you will find a schema with the whole pipeline for reference.
 * https://github.com/moonlight-stream/moonlight-qt/pull/1557
 *
 * \return void
 */
void D3D12VARenderer::enhanceAutoSelection()
{
    bool shader62support = isShader62supported();
    
    // We copy the value to not overwrite the user selection value while testing
    StreamingPreferences::SuperResolutionMode superResolutionMode = m_Preferences->superResolutionMode;

    m_VendorVSRenabled = false;
    m_VendorHDRenabled = false;
    m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
    m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
    m_RenderStep2 = RenderStep::NONE;
    
    // For Test, we default on Auto mode as it is made to work on both, SDR and HDR.
    // It avoids a fallback to Vulkan if in Debug mode an specific algorythm is selected which may not work in HDR (like FSR1).
    if (m_DecoderParams.testOnly) {
        superResolutionMode = StreamingPreferences::SRM_00;
    }

    // We first let the application select the estimated best-fit per GPU Vendor.
    // This is equivalent to StreamingPreferences::SRM_00 plus some vendor specifications.

    // AMD
    if(m_VideoEnhancement->isVendorAMD()){
        m_VendorVSRenabled = true;
        m_VendorHDRenabled = false;
        m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
        m_RenderStep1 = RenderStep::ALL_AMF;
        m_RenderStep2 = RenderStep::NONE;
        m_InfoUpscaler = "AMF FSR EASU";
        m_InfoSharpener = "AMF FSR RCAS";
        m_InfoAlgo = "AMF FSR1";
    }

    // Intel
    else if(m_VideoEnhancement->isVendorIntel()){
        if(m_IsIntegratedGPU){
            m_VendorVSRenabled = false;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
            m_RenderStep2 = RenderStep::NONE;
            m_InfoUpscaler = "Video Processor";
            m_InfoSharpener = "None";
            if (m_EdgeEnhancementValue > 0){
                m_InfoSharpener = "Video Processor";
            }
            m_InfoAlgo = "Video Processor";
        } else if(m_IsTexture10bits){
            m_VendorVSRenabled = false;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NIS;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_SHADER;
            m_InfoUpscaler = "NIS Upscaler";
            m_InfoSharpener = "NIS Sharpener";
            m_InfoAlgo = "Shader NIS";
        } else {

            // IntelVPL is only available for the architecture x64, we fallback to FSR1 only for ARM.
#if defined(HAVE_INTEL_VPL)
            // // NOTE: Current version is too slow (20ms per frame)
            // m_VendorVSRenabled = true;
            // m_VendorHDRenabled = false;
            // m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            // m_RenderStep1 = RenderStep::UPSCALE_VPL;
            // m_RenderStep2 = RenderStep::CONVERT_SHADER;
            // m_InfoUpscaler = "Intel VPL AI Super Resolution";
            // m_InfoSharpener = "Intel VPL";
            // m_InfoAlgo = "Intel VPL";
            
            m_VendorVSRenabled = false;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_SHADER;
            m_InfoUpscaler = "FSR1 EASU";
            m_InfoSharpener = "FRS1 RCAS";
            m_InfoAlgo = "Shader FSR1";
#else
            m_VendorVSRenabled = false;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_SHADER;
            m_InfoUpscaler = "FSR1 EASU";
            m_InfoSharpener = "FRS1 RCAS";
            m_InfoAlgo = "Shader FSR1";
#endif
        }
    }

    // NVIDIA
    else if(m_VideoEnhancement->isVendorNVIDIA()){
        if(isNvidiaVSRSupport()){
            // Nvidia Driver's optimization
            m_VendorVSRenabled = true;
            m_VendorHDRenabled = false; // It makes some screens darker
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_VSR;
            m_InfoUpscaler = "NVIDIA RTX Video Super Resolution";
            m_InfoSharpener = "Video Processor";
            m_InfoAlgo = "NVIDIA RTX Video Super Resolution";
        } else {
            // For GPUs which are not supporting VSR capability (like the GTX),
            // we switch to NIS.
            m_VendorVSRenabled = false;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NIS;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_SHADER;
            m_InfoUpscaler = "NIS Upscaler";
            m_InfoSharpener = "NIS Sharpener";
            m_InfoAlgo = "Shader NIS";
        }
    }

    // The user can force the algorithm used for Test/Debug purpose only, the production must be set to "auto"
    // User Interface: Hidden by default. It is visible only in Debug mode.
    // CLI: It is available via the parameter "super-resolution-mode" to force the algorythm to use

    switch (superResolutionMode) {

    case StreamingPreferences::SRM_01:
        // DRIVER
        m_VendorVSRenabled = true;
        m_VendorHDRenabled = true;
        m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
        m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
        m_RenderStep2 = RenderStep::NONE;
        if(m_VideoEnhancement->isVendorAMD()){
            m_VendorVSRenabled = true;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::ALL_AMF;
            m_RenderStep2 = RenderStep::NONE;
        } else if(m_VideoEnhancement->isVendorIntel()){
            m_VendorVSRenabled = true;
            m_VendorHDRenabled = false;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::UPSCALE_VPL;
            m_RenderStep2 = RenderStep::CONVERT_SHADER;
        } else if(m_VideoEnhancement->isVendorNVIDIA()){
            m_VendorVSRenabled = true;
            m_VendorHDRenabled = true;
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::CONVERT_SHADER;
            m_RenderStep2 = RenderStep::UPSCALE_VSR;
        }
        m_InfoUpscaler = "Vendor Driver Upscaler";
        m_InfoSharpener = "Vendor Driver Sharpener";
        m_InfoAlgo = "Vendor Driver";
        break;

    case StreamingPreferences::SRM_02:
        // VP_ONLY
        m_VendorVSRenabled = false;
        // m_VendorHDRenabled: Keep default setting
        m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
        m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
        m_RenderStep2 = RenderStep::NONE;
        m_InfoUpscaler = "Video Processor";
        m_InfoSharpener = "None";
        if (m_EdgeEnhancementValue > 0){
            m_InfoSharpener = "Video Processor";
        }
        m_InfoAlgo = "Video Processor";
        break;

    case StreamingPreferences::SRM_03:
        // FSR1 (Shader version)
        m_VendorVSRenabled = false;
        // m_VendorHDRenabled: Keep default setting
        m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
        m_RenderStep1 = RenderStep::CONVERT_SHADER;
        m_RenderStep2 = RenderStep::UPSCALE_SHADER;
        m_InfoUpscaler = "FSR1 EASU";
        m_InfoSharpener = "FRS1 RCAS";
        m_InfoAlgo = "Shader FSR1";
        break;

    case StreamingPreferences::SRM_04:
        // NIS
        m_VendorVSRenabled = false;
        // m_VendorHDRenabled: Keep default setting
        m_EnhancerType = D3D12VideoShaders::Enhancer::NIS;
        m_RenderStep1 = RenderStep::CONVERT_SHADER;
        m_RenderStep2 = RenderStep::UPSCALE_SHADER;
        m_InfoUpscaler = "NIS Upscaler";
        m_InfoSharpener = "NIS Sharpener";
        m_InfoAlgo = "Shader NIS";
        break;

    case StreamingPreferences::SRM_05:
        // RCAS (Only Sharpener)
        m_VendorVSRenabled = false;
        // m_VendorHDRenabled: Keep default setting
        m_EnhancerType = D3D12VideoShaders::Enhancer::RCAS;
        m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
        m_RenderStep2 = RenderStep::SHARPEN_SHADER;
        m_InfoUpscaler = "Video Processor";
        m_InfoSharpener = "RCAS Sharpener";
        m_InfoAlgo = "Video Processor RCAS";
        break;

    case StreamingPreferences::SRM_06:
        // NIS Sharpener
        m_VendorVSRenabled = false;
        // m_VendorHDRenabled: Keep default setting
        m_EnhancerType = D3D12VideoShaders::Enhancer::NIS_SHARPENER;
        m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
        m_RenderStep2 = RenderStep::SHARPEN_SHADER;
        m_InfoUpscaler = "Video Processor";
        m_InfoSharpener = "NIS Sharpener";
        m_InfoAlgo = "Video Processor NIS";
        break;

    default:
        break;
    }

    // Disable SDR->HDR feature if we receive HDR stream, or if the display is HDR off
    if(D3D12VARenderer::m_IsFrameHDR || !D3D12VARenderer::m_IsClientHDR){
        m_VendorHDRenabled = false;
    }

    // Disable VSR if we use Shader to upscale
    if(D3D12VideoShaders::isUpscaler(m_EnhancerType)){
        m_VendorVSRenabled = false;
    }

    // For unsuported shader 6.2 (old GPU), switch to Video Processor
    if(!shader62support){
        m_VendorVSRenabled = false;
        m_VendorHDRenabled = false;
        m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
        m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
        m_RenderStep2 = RenderStep::NONE;
        m_InfoUpscaler = "Video Processor";
        m_InfoSharpener = "None";
        if (m_EdgeEnhancementValue > 0){
            m_InfoSharpener = "Video Processor";
        }
        m_InfoAlgo = "Video Processor";
    }

    // Correct VSR
    m_VendorVSRenabled = false;
    switch (m_RenderStep1) {
    case RenderStep::ALL_AMF:
    case RenderStep::CONVERT_AMF:
    case RenderStep::UPSCALE_AMF:
    case RenderStep::UPSCALE_VSR:
    case RenderStep::UPSCALE_VPL:
        m_VendorVSRenabled = true;
        break;
    default:
        break;
    }
    switch (m_RenderStep2) {
    case RenderStep::ALL_AMF:
    case RenderStep::CONVERT_AMF:
    case RenderStep::UPSCALE_AMF:
    case RenderStep::UPSCALE_VSR:
    case RenderStep::UPSCALE_VPL:
        m_VendorVSRenabled = true;
        break;
    default:
        break;
    }
    
    // Test VSR, but do not activate
    if(m_VendorVSRenabled){
        if(m_VideoEnhancement->isVendorAMD()){
            m_VideoEnhancement->setVSRcapable(enableAMDVideoSuperResolution(false));
        } else if(m_VideoEnhancement->isVendorIntel()){
            m_VideoEnhancement->setVSRcapable(enableIntelVideoSuperResolution(false));
        } else if(m_VideoEnhancement->isVendorNVIDIA()){
            m_VideoEnhancement->setVSRcapable(enableNvidiaVideoSuperResolution(false));
        }
        m_VendorVSRenabled = m_VideoEnhancement->isVSRcapable();
        
        // We fallback to VideoProcessor
        if(!m_VendorVSRenabled){
            m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
            m_RenderStep1 = RenderStep::ALL_VIDEOPROCESSOR;
            m_RenderStep2 = RenderStep::NONE;
            m_InfoUpscaler = "Video Processor";
            m_InfoSharpener = "None";
            if (m_EdgeEnhancementValue > 0){
                m_InfoSharpener = "Video Processor";
            }
            m_InfoAlgo = "Video Processor";
        }
    }
    
    // Test HDR, but do not activate
    if(m_VendorHDRenabled){
        if(m_VideoEnhancement->isVendorAMD()){
            m_VideoEnhancement->setHDRcapable(enableAMDHDR(false));
        } else if(m_VideoEnhancement->isVendorIntel()){
            m_VideoEnhancement->setHDRcapable(enableIntelHDR(false));
        } else if(m_VideoEnhancement->isVendorNVIDIA()){
            m_VideoEnhancement->setHDRcapable(enableNvidiaHDR(false));
        }
        m_VendorHDRenabled = m_VideoEnhancement->isHDRcapable();
    }

    if(m_RenderStep1 == RenderStep::ALL_VIDEOPROCESSOR && m_EdgeEnhancementValue > 0){
        m_RenderStep2 = RenderStep::NONE;
        m_InfoSharpener = "Video Processor";
        m_InfoAlgo = "Video Processor";
    }
    
    // Note: Currently the YUV->RGB Shader is not accurate (wrong color), so we rely on the Video Processor to convert
    if(m_IsTextureYUV444 && m_RenderStep1 == RenderStep::CONVERT_SHADER){
        m_RenderStep1 = RenderStep::CONVERT_VIDEOPROCESSOR;
    }
    if(m_IsTextureYUV444 && m_RenderStep2 == RenderStep::CONVERT_SHADER){
        m_RenderStep2 = RenderStep::CONVERT_VIDEOPROCESSOR;
    }

    // At true, RenderStep2 is not needed
    m_SkipRenderStep2 = m_RenderStep2 == RenderStep::NONE;
    
    // Add statistics information
    m_VideoEnhancement->setRatio(static_cast<float>(m_OutputTextureInfo.height) / static_cast<float>(m_DecoderParams.textureHeight));
    if(m_VendorHDRenabled){
        m_InfoAlgo = m_InfoAlgo + " (SDR->HDR)";
    }
    m_VideoEnhancement->setAlgo(m_InfoAlgo);

    qInfo() << "Enhancer VSR       : " + std::to_string(m_VendorVSRenabled);
    qInfo() << "Enhancer SDR->HDR  : " + std::to_string(m_VendorHDRenabled);
    qInfo() << "Enhancer Upscaling : " + m_InfoUpscaler;
    qInfo() << "Enhancer Sharpening: " + m_InfoSharpener;
}

/**
 * \brief Enable Video Super-Resolution for AMD GPU
 *
 * This feature is available since this drive 22.3.1 (March 2022)
 * https://community.amd.com/t5/gaming/amd-software-24-1-1-amd-fluid-motion-frames-an-updated-ui-and/ba-p/656213
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableAMDVideoSuperResolution(bool activate, bool logInfo)
{
    // The feature is announced since Jan 23rd, 2024, with the driver 24.1.1 and on series 7000
    // https://community.amd.com/t5/gaming/amd-software-24-1-1-amd-fluid-motion-frames-an-updated-ui-and/ba-p/656213
    // But it is available as SDK since March 2022 (22.3.1) which means it might also work for series 5000 and 6000 (to be tested)
    // https://github.com/GPUOpen-LibrariesAndSDKs/AMF/blob/master/amf/doc/AMF_HQ_Scaler_API.md

    if (!m_VendorVSRenabled)
        activate = false;

    AMF_RESULT res;
    AMFCapsPtr amfCaps;
    AMFContextPtr baseContext;

    // We skip if already initialized
    if(m_AmfInitialized && activate)
        return true;

    AMF_SURFACE_FORMAT SurfaceFormatYUV;
    AMF_SURFACE_FORMAT SurfaceFormatRGB;
    AMFColor backgroundColor = AMFConstructColor(0, 0, 0, 255);
    AMF_COLOR_RANGE_ENUM amfColorRange = AMF_COLOR_RANGE_STUDIO;

    // AMF Context initialization
    res = g_AMFFactory.Init();
    if (res != AMF_OK) goto ErrorAMD;
    res = g_AMFFactory.GetFactory()->CreateContext(&baseContext);
    if (res != AMF_OK) goto ErrorAMD;
    res = baseContext->QueryInterface(AMFContext2::IID(), (void**)&m_AmfContext);
    if (res != AMF_OK) goto ErrorAMD;
    res = g_AMFFactory.GetFactory()->CreateComponent(m_AmfContext, AMFHQScaler, &m_AmfUpscalerRGB);
    if (res != AMF_OK) goto ErrorAMD;
    res = g_AMFFactory.GetFactory()->CreateComponent(m_AmfContext, AMFHQScaler, &m_AmfUpscalerYUV);
    if (res != AMF_OK) goto ErrorAMD;
    res = g_AMFFactory.GetFactory()->CreateComponent(m_AmfContext, AMFVideoConverter, &m_AmfVideoConverter);
    if (res != AMF_OK) goto ErrorAMD;
    res = g_AMFFactory.GetFactory()->CreateComponent(m_AmfContext, AMFVideoConverter, &m_AmfVideoConverterUpscaled);
    if (res != AMF_OK) goto ErrorAMD;

    res = m_AmfContext->InitDX12(m_Device.Get());
    if (res != AMF_OK) goto ErrorAMD;

    res = m_AmfContext->GetCompute(AMF_MEMORY_DX12, &m_AmfCompute);
    if (res != AMF_OK) goto ErrorAMD;

    // We are getting the command queue to work with fence
    m_AmfCommandQueue = static_cast<ID3D12CommandQueue*>(m_AmfCompute->GetNativeCommandQueue());

    // AMFHQScaler is the newest feature available (v1.4.33), so at least this one need to be accessible
    m_AmfUpscalerYUV->GetCaps(&amfCaps);
    if (amfCaps != nullptr && amfCaps->GetAccelerationType() == AMF_ACCEL_NOT_SUPPORTED) {
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "The hardware does not support needed AMD AMF capabilities.");
        goto ErrorAMD;
    }

    // Format initialization
    if(m_IsTextureYUV444){
        SurfaceFormatYUV = m_IsTexture10bits ? AMF_SURFACE_Y410 : AMF_SURFACE_AYUV;
    } else {
        SurfaceFormatYUV = m_IsTexture10bits ? AMF_SURFACE_P010 : AMF_SURFACE_NV12;
    }

    // Format initialization
    SurfaceFormatRGB = m_IsTexture10bits ? AMF_SURFACE_R10G10B10A2 : AMF_SURFACE_RGBA;

    if(m_Decoder.ColorRange & COLOR_RANGE_FULL){
        amfColorRange = AMF_COLOR_RANGE_FULL;
    }

    // Input YUV Surface initialization
    res = m_AmfContext->AllocSurface(AMF_MEMORY_DX12,
                                     SurfaceFormatYUV,
                                     m_DecoderParams.textureWidth,
                                     m_DecoderParams.textureHeight,
                                     &m_AmfSurfaceYUV);
    if (res != AMF_OK) goto ErrorAMD;

    // Input/Output Scaled YUV Surface initialization
    res = m_AmfContext->AllocSurface(AMF_MEMORY_DX12,
                                     SurfaceFormatYUV,
                                     m_OutputTextureInfo.width,
                                     m_OutputTextureInfo.height,
                                     &m_AmfSurfaceUpscaledYUV);
    if (res != AMF_OK) goto ErrorAMD;

    // Input/Output RGB Surface initialization
    res = m_AmfContext->AllocSurface(AMF_MEMORY_DX12,
                                     SurfaceFormatRGB,
                                     m_DecoderParams.textureWidth,
                                     m_DecoderParams.textureHeight,
                                     &m_AmfSurfaceRGB);
    if (res != AMF_OK) goto ErrorAMD;

    // Output Scaled RGB Surface initialization
    res = m_AmfContext->AllocSurface(AMF_MEMORY_DX12,
                                     SurfaceFormatRGB,
                                     m_OutputTextureInfo.width,
                                     m_OutputTextureInfo.height,
                                     &m_AmfSurfaceUpscaledRGB);
    if (res != AMF_OK) goto ErrorAMD;

    // RGB Upscale initialization
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_OutputTextureInfo.width, m_OutputTextureInfo.height));
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, AMF_MEMORY_DX12);
    // Do not use AMF_HQ_SCALER_ALGORITHM_VIDEOSR1_1, the picture is blurry, even with a ratio of 2.0
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_ALGORITHM, AMF_HQ_SCALER_ALGORITHM_VIDEOSR1_0);
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_KEEP_ASPECT_RATIO, true);
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_FILL, true);
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_FILL_COLOR, backgroundColor);
    // We only apply sharpening when the picture is scaled (0 = Most sharpened / 2.00 = Not sharpened)
    if (m_OutputTextureInfo.width == m_DecoderParams.textureWidth && m_OutputTextureInfo.height == m_DecoderParams.textureHeight){
        m_AmfUpscalerSharpness = false;
    } else {
        m_AmfUpscalerSharpness = true;
    }
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_SHARPNESS, m_AmfUpscalerSharpness ? 0.50 : 2.00);
    m_AmfUpscalerRGB->SetProperty(AMF_HQ_SCALER_FRAME_RATE, m_DecoderParams.frameRate);
    // Initialize with the size of the texture that will be input
    res = m_AmfUpscalerRGB->Init(SurfaceFormatRGB,
                                 m_DecoderParams.textureWidth,
                                 m_DecoderParams.textureHeight);
    if (res != AMF_OK) goto ErrorAMD;
    m_AmfUpscalerRGB->Optimize(nullptr);

    // YUV Upscale initialization
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_OUTPUT_SIZE, ::AMFConstructSize(m_OutputTextureInfo.width, m_OutputTextureInfo.height));
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_ENGINE_TYPE, AMF_MEMORY_DX12);
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_ALGORITHM, AMF_HQ_SCALER_ALGORITHM_VIDEOSR1_0);
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_KEEP_ASPECT_RATIO, true);
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_FILL, true);
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_FILL_COLOR, backgroundColor);
    // We only apply sharpening when the picture is scaled (0 = Most sharpened / 2.00 = Not sharpened)
    if (m_OutputTextureInfo.width == m_DecoderParams.textureWidth && m_OutputTextureInfo.height == m_DecoderParams.textureHeight){
        m_AmfUpscalerSharpness = false;
    } else {
        m_AmfUpscalerSharpness = true;
    }
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_SHARPNESS, m_AmfUpscalerSharpness ? 0.50 : 2.00);
    m_AmfUpscalerYUV->SetProperty(AMF_HQ_SCALER_FRAME_RATE, m_DecoderParams.frameRate);
    // Initialize with the size of the texture that will be input
    res = m_AmfUpscalerYUV->Init(SurfaceFormatYUV,
                                 m_DecoderParams.textureWidth,
                                 m_DecoderParams.textureHeight);
    if (res != AMF_OK) goto ErrorAMD;
    m_AmfUpscalerYUV->Optimize(nullptr);

    // Convert YUV to RGB
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, AMF_MEMORY_DX12);
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, SurfaceFormatRGB);
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_FILL, true);
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_FILL_COLOR, backgroundColor);
    // Note: For unknown reason, the HDR rendering is correct while keeping BT709 color space.
    // If I use the HDR setting, the coloring is too bright.
    if(m_AmfHdrColorSpaceEnabled && D3D12VARenderer::m_IsFrameHDR){
        // Input P010, RGB BT.2020 with PQ (HDR10), limited/full range
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT2020);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_RANGE, amfColorRange);
        // Output R10G10B10A2, RGB BT.2020 with PQ (HDR10), full range
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_2020);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT2020);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_RANGE, AMF_COLOR_RANGE_FULL);
    } else {
        // Input NV12 = YUV BT.709, limited/full range
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT709);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_RANGE, amfColorRange);
        // Output RGBA = RGB BT.709, full range
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_709);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT709);
        m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_RANGE, AMF_COLOR_RANGE_FULL);
    }
    // Initialize with the size of the output texture
    res = m_AmfVideoConverter->Init(SurfaceFormatYUV,
                                    m_DecoderParams.textureWidth,
                                    m_DecoderParams.textureHeight);
    if (res != AMF_OK) goto ErrorAMD;
    m_AmfVideoConverter->Optimize(nullptr);

    // Convert Upscaled YUV to RGB
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_MEMORY_TYPE, AMF_MEMORY_DX12);
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_FORMAT, SurfaceFormatRGB);
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_FILL, true);
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_FILL_COLOR, backgroundColor);
    if(m_AmfHdrColorSpaceEnabled && D3D12VARenderer::m_IsFrameHDR){
        // Input P010, RGB BT.2020 with PQ (HDR10), limited/full range
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT2020);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_RANGE, amfColorRange);
        // Output R10G10B10A2, RGB BT.2020 with PQ (HDR10), full range
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_FULL_2020);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_SMPTE2084);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT2020);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_RANGE, AMF_COLOR_RANGE_FULL);
    } else {
        // Input NV12 = YUV BT.709, limited range
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT709);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_COLOR_RANGE, amfColorRange);
        // Output RGBA = RGB BT.709, full range
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_COLOR_PROFILE, AMF_VIDEO_CONVERTER_COLOR_PROFILE_709);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_TRANSFER_CHARACTERISTIC, AMF_COLOR_TRANSFER_CHARACTERISTIC_BT709);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_PRIMARIES, AMF_COLOR_PRIMARIES_BT709);
        m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_COLOR_RANGE, AMF_COLOR_RANGE_FULL);
    }
    // Initialize with the size of the output texture
    res = m_AmfVideoConverterUpscaled->Init(SurfaceFormatYUV,
                                            m_OutputTextureInfo.width,
                                            m_OutputTextureInfo.height);
    if (res != AMF_OK) goto ErrorAMD;
    m_AmfVideoConverterUpscaled->Optimize(nullptr);

    if(!activate){
        // Up Scaler
        m_AmfUpscalerRGB->Terminate();
        m_AmfUpscalerRGB = nullptr;
        m_AmfUpscalerYUV->Terminate();
        m_AmfUpscalerYUV = nullptr;
        // Converter
        m_AmfVideoConverter->Terminate();
        m_AmfVideoConverter = nullptr;
        m_AmfVideoConverterUpscaled->Terminate();
        m_AmfVideoConverterUpscaled = nullptr;
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

ErrorAMD:
    if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD Video Super Resolution failed.");
    m_AmfInitialized = false;
    return false;
}

/**
 * \brief Enable Video Super-Resolution for Intel GPU
 *
 * The AI Super Resolution is available as an experimental mode, it needs to enable ONEVPL_EXPERIMENTAL.
 * https://intel.github.io/libvpl/latest/API_ref/VPL_structs_vpp.html#mfxextvppaisuperresolution
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableIntelVideoSuperResolution(bool activate, bool logInfo)
{
    // IntelVPL is only available for the architecture x64
#if !defined(HAVE_INTEL_VPL)
    m_VendorVSRenabled = false;
#endif
    if (!m_VendorVSRenabled) {
        activate = false;
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution disabled");
        m_IntelInitialized = false;
        return false;
    }
    
    // Reset
    if (m_VppSession) {
        MFXVideoVPP_Close(m_VppSession);
        MFXClose(m_VppSession);
        m_VppSession = nullptr;
    }
    
    if (!m_D3D11Device)
        return false;
    
    ComPtr<ID3D11Multithread> pMultithread;
    HRESULT hr = m_D3D11Device.As(&pMultithread);
    if (SUCCEEDED(hr)) {
        pMultithread->SetMultithreadProtected(true);
        qInfo() << "VPL: D3D11: Multithread protection enabled.";
    } else {
        qInfo() << "VPL: D3D11: Failed to enable multithread protection.";
    }
    
    mfxHDL deviceHandle = (mfxHDL)m_D3D11Device.Get();
    
    if (!m_VendorVSRenabled)
        activate = false;
    
    // Safe helper : zero frameInfo first
    auto zeroFrame = [](mfxFrameInfo &fi){ memset(&fi, 0, sizeof(fi)); };
    
    mfxStatus sts;
    
    uint fourCC = MFX_FOURCC_NV12;
    if (m_IsTextureYUV444) {
        fourCC = MFX_FOURCC_AYUV;
    }
    
    // VPP Parameters
    m_VppParams = {};
    zeroFrame(m_VppParams.vpp.In);
    zeroFrame(m_VppParams.vpp.Out);
    
    m_VppParams.vpp.In.FourCC = fourCC;
    m_VppParams.vpp.In.ChromaFormat = m_IsTextureYUV444 ? MFX_CHROMAFORMAT_YUV444 : MFX_CHROMAFORMAT_YUV420;
    m_VppParams.vpp.In.FrameRateExtN = m_DecoderParams.frameRate;
    m_VppParams.vpp.In.FrameRateExtD = 1;
    m_VppParams.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_VppParams.vpp.In.CropW = m_DecoderParams.textureWidth;
    m_VppParams.vpp.In.CropH = m_DecoderParams.textureHeight;
    m_VppParams.vpp.In.CropX = 0;
    m_VppParams.vpp.In.CropY = 0;
    m_VppParams.vpp.In.Width = ALIGN16(m_VppParams.vpp.In.CropW);
    m_VppParams.vpp.In.Height = ALIGN16(m_VppParams.vpp.In.CropH);
    
    m_VppParams.vpp.Out.FourCC = fourCC;
    m_VppParams.vpp.Out.ChromaFormat = m_IsTextureYUV444 ? MFX_CHROMAFORMAT_YUV444 : MFX_CHROMAFORMAT_YUV420;
    m_VppParams.vpp.Out.FrameRateExtN = m_DecoderParams.frameRate;
    m_VppParams.vpp.Out.FrameRateExtD = 1;
    m_VppParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_VppParams.vpp.Out.CropW = m_OutputTextureInfo.width;
    m_VppParams.vpp.Out.CropH = m_OutputTextureInfo.height;
    m_VppParams.vpp.Out.CropX = 0;
    m_VppParams.vpp.Out.CropY = 0;
    m_VppParams.vpp.Out.Width = ALIGN16(m_VppParams.vpp.Out.CropW);
    m_VppParams.vpp.Out.Height = ALIGN16(m_VppParams.vpp.Out.CropH);
    
    m_VppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    m_VppParams.AsyncDepth = 2;
    
    m_VppParams.NumExtParam = 0;
    m_VppParams.ExtParam = nullptr;
    
    std::array<mfxExtBuffer*, 2> extParams = {};
    
    // AI Super Resolution
    // NOTE: Not suitable for real-time streaming.
    // It is activated when the scaling is over x1.40.
    // Currently too slow on a A380, 20ms (720p to 1440p) with AlGORITHM_1, and 40ms with AlGORITHM_2.
    // The picture quality is too flat, it looses details, it's good for Anime but not for games.
    mfxExtVPPAISuperResolution aiSuperResolution = {};
    aiSuperResolution.Header.BufferId = MFX_EXTBUFF_VPP_AI_SUPER_RESOLUTION;
    aiSuperResolution.Header.BufferSz = sizeof(mfxExtVPPAISuperResolution);
    aiSuperResolution.SRMode = MFX_AI_SUPER_RESOLUTION_MODE_DEFAULT;
    aiSuperResolution.SRAlgorithm = MFX_AI_SUPER_RESOLUTION_ALGORITHM_DEFAULT;
    extParams[m_VppParams.NumExtParam++] = (mfxExtBuffer*)&aiSuperResolution;
    
    // Fallback to Scaling quality method
    // NOTE: Same quality as Video Processor, but slower than Video Processor
    mfxExtVPPScaling scaling = {};
    scaling.Header.BufferId = MFX_EXTBUFF_VPP_SCALING;
    scaling.Header.BufferSz = sizeof(mfxExtVPPScaling);
    scaling.ScalingMode = MFX_SCALING_MODE_QUALITY;
    // extParams[m_VppParams.NumExtParam++] = (mfxExtBuffer*)&scaling;
    
    m_VppParams.ExtParam = extParams.data();
    
    mfxConfig mfxCfg[3];
    mfxVariant mfxVar[3];
    
    m_VppLoader = MFXLoad();
    if (m_VppLoader == NULL) goto ErrorIntel;
    
    // Hardware
    mfxCfg[0] = MFXCreateConfig(m_VppLoader);
    mfxVar[0].Version.Version = MFX_VARIANT_VERSION;
    mfxVar[0].Type = MFX_VARIANT_TYPE_U32;
    mfxVar[0].Data.U32 = MFX_IMPL_TYPE_HARDWARE;
    sts = MFXSetConfigFilterProperty(mfxCfg[0], (mfxU8 *)"mfxImplDescription.Impl", mfxVar[0]);
    if (sts != MFX_ERR_NONE) goto ErrorIntel;
    
    // D3D11
    mfxCfg[1] = MFXCreateConfig(m_VppLoader);
    mfxVar[1].Version.Version = MFX_VARIANT_VERSION;
    mfxVar[1].Type = MFX_VARIANT_TYPE_U32;
    mfxVar[1].Data.U32 = MFX_IMPL_VIA_D3D11;
    sts = MFXSetConfigFilterProperty(mfxCfg[1], (mfxU8 *)"mfxImplDescription.AccelerationMode", mfxVar[1]);
    if (sts != MFX_ERR_NONE) goto ErrorIntel;
    
    // Session
    sts = MFXCreateSession(m_VppLoader, 0, &m_VppSession);
    if (sts != MFX_ERR_NONE) goto ErrorIntel;
    
    // IntelVPL needs multithreading enabled
    MFXVideoCORE_SetHandle(m_VppSession, MFX_HANDLE_D3D11_DEVICE, deviceHandle);
    
    sts = MFXVideoVPP_Query(m_VppSession, &m_VppParams, &m_VppParams);
    if (sts == MFX_ERR_UNSUPPORTED) {
        // Fallback to Quality scaling
        
        if(m_VppSession){
            MFXVideoVPP_Close(m_VppSession);
            MFXClose(m_VppSession);
            m_VppSession = nullptr;
        }
        
        // Scaling
        mfxCfg[2] = MFXCreateConfig(m_VppLoader);
        mfxVar[2].Version.Version = MFX_VARIANT_VERSION;
        mfxVar[2].Type = MFX_VARIANT_TYPE_U32;
        mfxVar[2].Data.U32 = MFX_EXTBUFF_VPP_SCALING;
        sts = MFXSetConfigFilterProperty(mfxCfg[2], (mfxU8 *)"mfxImplDescription.mfxVPPDescription.filter.FilterFourCC", mfxVar[2]);
        if (sts != MFX_ERR_NONE) goto ErrorIntel;
        
        extParams[0] = (mfxExtBuffer*)&scaling;
        m_VppParams.NumExtParam = 1;
        m_VppParams.ExtParam = extParams.data();
        
        // Session
        sts = MFXCreateSession(m_VppLoader, 0, &m_VppSession);
        if (sts != MFX_ERR_NONE) goto ErrorIntel;
        
        // IntelVPL needs multithreading enabled
        MFXVideoCORE_SetHandle(m_VppSession, MFX_HANDLE_D3D11_DEVICE, deviceHandle);
        
    } else if (sts != MFX_ERR_NONE) {
        qInfo() << "MFXVideoVPP_Query failed:" << sts;
        goto ErrorIntel;
    }
    
    // VPP Initialization
    sts = MFXVideoVPP_Init(m_VppSession, &m_VppParams);
    if(MFX_ERR_NONE != sts){
        qInfo() << "VPP Initialization failed:" << sts;
        goto ErrorIntel;
    }
    
    MFXVideoVPP_QueryIOSurf(m_VppSession, &m_VppParams, m_VppReq);
    
    // Memory Interface
    MFXGetMemoryInterface(m_VppSession, &m_MemoryInterface);
    
    MFXUnload(m_VppLoader);
    
    if (activate) {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution enabled");
    } else {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution disabled");
    }
    
    m_IntelInitialized = activate;
    
    return true;
    

ErrorIntel:
    if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel Video Super Resolution failed.");
    m_IntelInitialized = false;
    MFXUnload(m_VppLoader);
    if (m_VppSession) {
        MFXVideoVPP_Close(m_VppSession);
        MFXClose(m_VppSession);
        m_VppSession = nullptr;
    }
    return false;
}

/**
 * \brief Enable Video Super-Resolution for NVIDIA
 *
 * This feature is available starting from series NVIDIA RTX 2000 and GeForce driver 545.84 (Oct 17, 2023)
 * https://catalog.ngc.nvidia.com/orgs/nvidia/teams/multimedia/models/dlpp
 *
 *
 * \param bool activate Default is true, at true it enables the use of Video Super-Resolution feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableNvidiaVideoSuperResolution(bool activate, bool logInfo)
{
    if (!m_VendorVSRenabled)
        activate = false;
    
    NVSDK_NGX_Result ResultVSR;
    
    // Reset
    NVSDK_NGX_D3D12_ReleaseFeature(m_VSRFeature);
    m_VSRFeature = nullptr;
    NVSDK_NGX_D3D12_DestroyParameters(m_VSRngxParameters);
    m_VSRngxParameters = {};
    
    // init NGX SDK
    NVSDK_NGX_Result Status = NVSDK_NGX_D3D12_Init(APP_ID, APP_PATH, m_Device.Get());
    if (NVSDK_NGX_FAILED(Status)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    // Get NGX parameters interface (managed and released by NGX)
    Status = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_VSRngxParameters);
    if (NVSDK_NGX_FAILED(Status)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    // Now check if VSR is available on the system
    int VSRAvailable = 0;
    ResultVSR = m_VSRngxParameters->Get(NVSDK_NGX_Parameter_VSR_Available, &VSRAvailable);
    if (!VSRAvailable){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    m_GraphicsCommandAllocator->Reset();
    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
    
    // Create the VSR feature instance 
    NVSDK_NGX_Feature_Create_Params VSRCreateParams = {};
    ResultVSR = NGX_D3D12_CREATE_VSR_EXT(m_GraphicsCommandList.Get(), 1, 1, &m_VSRFeature, m_VSRngxParameters, &VSRCreateParams);
    
    m_hr = m_GraphicsCommandList->Close();
    if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
    m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);
    
    waitForGraphics();
    m_GraphicsCommandAllocator->Reset();
    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
    
    if (NVSDK_NGX_FAILED(ResultVSR)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }

    if (activate) {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution enabled");
    } else {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution disabled");
    }

    m_NvidiaInitialized = activate;
    return true;
}

/**
 * \brief Enable HDR for AMD GPU
 *
 * This feature is not available for AMD, and has not yet been announced (by Dec 1st, 2025)
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableAMDHDR(bool activate, bool logInfo)
{
    if (!m_VendorHDRenabled)
        activate = false;

    // [TODO] Feature not yet announced

    if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "AMD HDR capability is not yet supported by your client's GPU.");
    return false;
}

/**
 * \brief Enable HDR for Intel GPU
 *
 * This feature is not available for Intel, and has not yet been announced (by Dec 1st, 2025)
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableIntelHDR(bool activate, bool logInfo)
{
    if (!m_VendorHDRenabled)
        activate = false;

    // [TODO] Feature not yet announced

    if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Intel HDR capability is not yet supported by your client's GPU.");
    return false;
}

/**
 * \brief Enable HDR for NVIDIA
 *
 * This feature is available starting from series NVIDIA RTX 2000 and GeForce driver 545.84 (Oct 17, 2023)
 *
 * Values from Chromium source code:
 * https://chromium.googlesource.com/chromium/src/+/master/ui/gl/swap_chain_presenter.cc
 *
 * \param bool activate Default is true, at true it enables the use of HDR feature
 * \param bool logInfo Default is true, at true is displays the result in the console logs
 * \return bool Return true if the capability is available
 */
bool D3D12VARenderer::enableNvidiaHDR(bool activate, bool logInfo)
{
    if (!m_VendorHDRenabled)
        activate = false;
    
    NVSDK_NGX_Result ResultTrueHDR;
    
    // Reset
    NVSDK_NGX_D3D12_ReleaseFeature(m_TrueHDRFeature);
    m_TrueHDRFeature = nullptr;
    NVSDK_NGX_D3D12_DestroyParameters(m_TrueHDRngxParameters);
    m_TrueHDRngxParameters = {};
    
    // init NGX SDK
    NVSDK_NGX_Result Status = NVSDK_NGX_D3D12_Init(APP_ID, APP_PATH, m_Device.Get());
    if (NVSDK_NGX_FAILED(Status)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    // Get NGX parameters interface (managed and released by NGX)
    Status = NVSDK_NGX_D3D12_GetCapabilityParameters(&m_TrueHDRngxParameters);
    if (NVSDK_NGX_FAILED(Status)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    // Now check if TrueHDR is available on the system
    int TrueHDRAvailable = 0;
    ResultTrueHDR = m_TrueHDRngxParameters->Get(NVSDK_NGX_Parameter_TrueHDR_Available, &TrueHDRAvailable);
    if (!TrueHDRAvailable){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    m_GraphicsCommandAllocator->Reset();
    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);

    // Create the TrueHDR feature instance 
    NVSDK_NGX_Feature_Create_Params TrueHDRCreateParams = {};
    ResultTrueHDR = NGX_D3D12_CREATE_TRUEHDR_EXT(m_GraphicsCommandList.Get(), 1, 1, &m_TrueHDRFeature, m_TrueHDRngxParameters, &TrueHDRCreateParams);

    m_hr = m_GraphicsCommandList->Close();
    if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }
    
    ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
    m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);
    
    waitForGraphics();
    m_GraphicsCommandAllocator->Reset();
    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
    
    if (NVSDK_NGX_FAILED(ResultTrueHDR)){
        if(logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX Video Super Resolution failed.");
        return false;
    }

    if (activate) {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX HDR enabled");
    } else {
        if (logInfo) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "NVIDIA RTX HDR disabled");
    }

    return true;
}

/**
 * \brief Find an unlocked surface in the pool
 *
 * Searches through the surface pool to find the first surface
 * that is either uninitialized or not currently locked.
 *
 * \param std::vector<VppSurface>& pool Surface pool to search
 * \return VppSurface* Pointer to unlocked surface, or nullptr if none found
 */
D3D12VARenderer::VppSurface* D3D12VARenderer::findUnlockedSurface(std::vector<VppSurface>& pool)
{
    for (auto& s : pool) {
        if (!s.surface || s.surface->Data.Locked == 0)
            return &s;
    }
    return nullptr;
}

/**
 * \brief Enable Video Super-Resolution for AMD GPU
 *
 * Verify if the GPU is able to support Shader 6.2 version to support half-precision feature
 *
 * \return bool Return true if the shader 6.2+ is supported
 */
bool D3D12VARenderer::isShader62supported()
{
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_2 };

    m_hr = m_Device->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_MODEL,
        &shaderModel,
        sizeof(shaderModel)
        );

    if(SUCCEEDED(m_hr)){
        if(shaderModel.HighestShaderModel >= D3D_SHADER_MODEL_6_2){
            qInfo() << "Shader Model 6.2 is supported";
            return true;
        }
    }

    qInfo() << "Shader Model 6.2 is not supported";
    return false;
}

/**
 * \brief Check is the Nvidia GPU is supporting Video Super Resolution
 *
 * This tells if the GPU is a RTX 2000+, which starts to support Video Super Resolution feature.
 * Identification is based on DX12 Mesh Shader feature.
 *
 * \return bool Return true if the GPU is RTX2000+
 */
bool D3D12VARenderer::isNvidiaVSRSupport()
{
    if(!m_VideoEnhancement->isVendorNVIDIA()){
        return false;
    }
    
    std::wstring wdesc(m_AdapterDesc.Description);
    std::string description(wdesc.begin(), wdesc.end());

    // Check if the description contains " RTX ", case-insensitive
    // This does cover all RTX GPUs
    if (std::regex_search(description, std::regex(" RTX ", std::regex_constants::icase)))
        return true;

    // Check Mesh Shader support (tier 1 minimum) which starts from RTX 3000+
    // This does cover any future Nvidia GPU which may not contains RTX in its description
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    if (SUCCEEDED(m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7))))
    {
        if (options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1) {
            return true;
        }
    }

    return false;
}

/**
 * \brief Set HDR capabilities
 *
 * Apply HDR metadata consistently across the processing pipeline to ensure
 * the final output is correctly rendered on HDR-capable displays.
 *
 * \param bool enabled At true it enables the HDR settings
 * \return void
 */
void D3D12VARenderer::setHdrMode(bool enabled)
{
    // Prepare HDR Meta Data for Streamed content
    bool streamSet = false;
    SS_HDR_METADATA hdrMetadata;
    if (enabled && LiGetHdrMetadata(&hdrMetadata)) {
        m_StreamHDRMetaData = {};
        m_StreamHDRMetaData.RedPrimary[0] = hdrMetadata.displayPrimaries[0].x;
        m_StreamHDRMetaData.RedPrimary[1] = hdrMetadata.displayPrimaries[0].y;
        m_StreamHDRMetaData.GreenPrimary[0] = hdrMetadata.displayPrimaries[1].x;
        m_StreamHDRMetaData.GreenPrimary[1] = hdrMetadata.displayPrimaries[1].y;
        m_StreamHDRMetaData.BluePrimary[0] = hdrMetadata.displayPrimaries[2].x;
        m_StreamHDRMetaData.BluePrimary[1] = hdrMetadata.displayPrimaries[2].y;
        m_StreamHDRMetaData.WhitePoint[0] = hdrMetadata.whitePoint.x;
        m_StreamHDRMetaData.WhitePoint[1] = hdrMetadata.whitePoint.y;
        m_StreamHDRMetaData.MaxMasteringLuminance = hdrMetadata.maxDisplayLuminance;
        m_StreamHDRMetaData.MinMasteringLuminance = hdrMetadata.minDisplayLuminance;
        
        // As the Content is unknown since it is streamed, MaxCLL and MaxFALL cannot be evaluated from the source on the fly,
        // therefore streamed source returns 0 as value for both. We default at 1000/400 (standard);
        m_StreamHDRMetaData.MaxContentLightLevel = 1000;
        m_StreamHDRMetaData.MaxFrameAverageLightLevel = 4000;
        
        setAMDHdr();
        
        // It seems that there is no effect
        m_SwapChain->SetHDRMetaData(
            DXGI_HDR_METADATA_TYPE_HDR10,
            sizeof(m_StreamHDRMetaData),
            &m_StreamHDRMetaData
            );
        
        streamSet = true;
    }
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Set stream HDR mode: %s", streamSet ? "enabled" : "disabled");
}

/**
 * \brief Apply AMD AMF HDR metadata
 *
 * Configures and propagates HDR10 static metadata to AMD AMF video converters,
 * ensuring correct HDR rendering for PQ (ST 2084) encoded frames.
 *
 * \return void
 */
void D3D12VARenderer::setAMDHdr()
{
    if(!m_AmfInitialized || !m_AmfHdrColorSpaceEnabled || !D3D12VARenderer::m_IsFrameHDR)
        return;
    
    AMF_RESULT res;
    
    // HDR Input Metadata
    AMFHDRMetadata hdr = {};
    
    // Primaires (exemple BT.2020)
    hdr.redPrimary[0]   = m_StreamHDRMetaData.RedPrimary[0];
    hdr.redPrimary[1]   = m_StreamHDRMetaData.RedPrimary[1];
    hdr.greenPrimary[0] = m_StreamHDRMetaData.GreenPrimary[0];
    hdr.greenPrimary[1] = m_StreamHDRMetaData.GreenPrimary[1];
    hdr.bluePrimary[0]  = m_StreamHDRMetaData.BluePrimary[0];
    hdr.bluePrimary[1]  = m_StreamHDRMetaData.BluePrimary[1];
    
    // White point (D65)
    hdr.whitePoint[0]   = m_StreamHDRMetaData.WhitePoint[0];
    hdr.whitePoint[1]   = m_StreamHDRMetaData.WhitePoint[1];
    
    // Luminance mastering
    hdr.maxMasteringLuminance = 10000 * m_StreamHDRMetaData.MaxMasteringLuminance;
    hdr.minMasteringLuminance = m_StreamHDRMetaData.MinMasteringLuminance;
    
    // Light Levels
    hdr.maxContentLightLevel = m_StreamHDRMetaData.MaxContentLightLevel;
    hdr.maxFrameAverageLightLevel = m_StreamHDRMetaData.MaxFrameAverageLightLevel;
    
    res = m_AmfContext->AllocBuffer(AMF_MEMORY_HOST,
                                    sizeof(AMFHDRMetadata),
                                    &m_HdrBuffer);
    if (res != AMF_OK) return;
    memcpy(m_HdrBuffer->GetNative(), &hdr, sizeof(AMFHDRMetadata));
    
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_INPUT_HDR_METADATA, m_HdrBuffer);
    m_AmfVideoConverter->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_HDR_METADATA, m_HdrBuffer);
    
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_INPUT_HDR_METADATA, m_HdrBuffer);
    m_AmfVideoConverterUpscaled->SetProperty(AMF_VIDEO_CONVERTER_OUTPUT_HDR_METADATA, m_HdrBuffer);
}

/**
 * \brief Set the decoder engine
 *
 * Set the decoder engine
 *
 * \param AVCodecContext* context
 * \param AVDictionary** options
 * \return bool
 */
bool D3D12VARenderer::prepareDecoderContext(AVCodecContext* context, AVDictionary** options)
{
    context->hw_device_ctx = av_buffer_ref(m_HwDeviceContext);

    if (m_IsFrameD3D11) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using D3D11VA decoding with D3D12VA accelerated renderer");
    } else {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Using D3D12VA accelerated renderer");
    }

    return true;
}

/**
 * \brief Set the decoder frame format
 *
 * Set the decoder frame format
 *
 * \param AVCodecContext* context
 * \param AVPixelFormat pixelFormat
 * \return bool
 */
bool D3D12VARenderer::prepareDecoderContextInGetFormat(AVCodecContext *context, AVPixelFormat pixelFormat)
{
    int err;
    
    // Create a new hardware frames context suitable for decoding our specified format
    av_buffer_unref(&context->hw_frames_ctx);
    err = avcodec_get_hw_frames_parameters(context, m_HwDeviceContext, pixelFormat, &context->hw_frames_ctx);
    if (err < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to get hwframes context parameters: %d",
                     err);
        return false;
    }
    
    AVHWFramesContext* framesContext = (AVHWFramesContext*)context->hw_frames_ctx->data;

    // We can have up to 16 reference frames plus a working surface
    framesContext->initial_pool_size = DECODER_BUFFER_POOL_SIZE;
    
    framesContext->sw_format = m_Decoder.AVFormat;
    framesContext->width = m_FrameWidth;
    framesContext->height = m_FrameHeight;
    
    if (m_IsFrameD3D11) {
        // Use D3D11va to decode
        
        framesContext->format = AV_PIX_FMT_D3D11;
        
        AVD3D11VAFramesContext* d3d11vaFramesContext = (AVD3D11VAFramesContext*)framesContext->hwctx;
        
        d3d11vaFramesContext->BindFlags |= D3D11_BIND_DECODER;
        d3d11vaFramesContext->MiscFlags |= D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        
    } else {
        // Use D3D12va to decode
        
        framesContext->format = AV_PIX_FMT_D3D12;

        AVD3D12VAFramesContext* d3d12vaFramesContext = (AVD3D12VAFramesContext*)framesContext->hwctx;

        d3d12vaFramesContext->flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        d3d12vaFramesContext->format = m_Decoder.Format;
        
    }
    
    err = av_hwframe_ctx_init(context->hw_frames_ctx);
    if (err < 0) {
        av_buffer_unref(&context->hw_frames_ctx);
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed initialize hwframes context: %d",
                     err);
        return false;
    }
    
    return true;
}

/**
 * \brief Notified if the pipeline supports live window changes
 *
 * Always at false to force the recreation of decoder/renderer
 *
 * \param PWINDOW_STATE_CHANGE_INFO stateInfo
 * \return bool, Always returns false
 */
bool D3D12VARenderer::notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO stateInfo)
{
    // We force to recreate the renderer
    return !(stateInfo->stateChangeFlags & (WINDOW_STATE_CHANGE_DISPLAY | WINDOW_STATE_CHANGE_SIZE));
}

/**
 * \brief Get Renderer attributes
 *
 * Get Renderer attributes activated for the renderer.
 *
 * \return int, Return Attributes
 */
int D3D12VARenderer::getRendererAttributes()
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

/**
 * \brief Add capabilities to the decoder
 *
 * By default we ignore the reference frame for HEVC and AV1 as adding 1 frame equals to high-latency.
 * H264 doesn't use reference frame.
 *
 * \return int, Returns the value as an integer of capabilities added
 */
int D3D12VARenderer::getDecoderCapabilities()
{
    return CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC |
           CAPABILITY_REFERENCE_FRAME_INVALIDATION_AV1;
}

/**
 * \brief Get decoder color space
 *
 * Get decoder color space
 *
 * \return int, Returns the value of the color space
 */
int D3D12VARenderer::getDecoderColorspace()
{
    return D3D12VARenderer::m_IsFrameHDR ? COLORSPACE_REC_2020 : COLORSPACE_REC_709;
}

/**
 * \brief Get decoder color range
 *
 * Get decoder color space.
 *
 * \return int, Always returns the value of the color range full
 */
int D3D12VARenderer::getDecoderColorRange()
{
    // Compare to Limited, Full has only a additional bandwidth cost is about 3% for SDR, it avoid banding and have better color accuracy.
    return COLOR_RANGE_FULL;
}

/**
 * \brief Lock the rendering context
 *
 * Inform the decoder FFmpeg that the DeviceContext is in used.
 *
 * \param void *lock_ctx
 * \return void
 */
void D3D12VARenderer::lockContext(void *lock_ctx)
{
    auto me = (D3D12VARenderer*)lock_ctx;

    SDL_LockMutex(me->m_ContextLock);
}

/**
 * \brief Unlock the rendering context
 *
 * Inform the decoder FFmpeg that the DeviceContext is free.
 *
 * \param void *lock_ctx
 * \return void
 */
void D3D12VARenderer::unlockContext(void *lock_ctx)
{
    auto me = (D3D12VARenderer*)lock_ctx;
    
    SDL_UnlockMutex(me->m_ContextLock);
}

/**
 * \brief Initialize the information of the best-fit Adapter
 *
 * In case of multiple GPUs, apply the most appropriate GPU available based on Video Super Resolution capabilities
 * and priority of Vendor implementation status (NVIDIA -> AMD -> Intel -> Others).
 *
 * \return bool Returns false if the initializatin failed
 */
bool D3D12VARenderer::initialiazeAdapterInformation()
{
    int adapterIndex = 0;
    int outputIndex  = 0;

    // Identify the GPU plug to the display
    {
        if (SDL_DXGIGetOutputInfo(SDL_GetWindowDisplayIndex(m_DecoderParams.window),
                                  &adapterIndex,
                                  &outputIndex))
        {
            m_AdapterIndex = static_cast<UINT>(adapterIndex);
            m_OutputIndex  = static_cast<UINT>(outputIndex);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "SDL_DXGIGetOutputInfo() failed: %s",
                         SDL_GetError());
            return false;
        }
    }

    m_hr = m_Factory->EnumAdapters1(m_AdapterIndex, &m_Adapter);
    if(!verifyHResult(m_hr, "m_Factory->EnumAdapters1(m_AdapterIndex, &m_Adapter);")){
        return false;
    }

    m_hr = m_Adapter->GetDesc1(&m_AdapterDesc);
    if(!verifyHResult(m_hr, "m_Adapter->GetDesc1(&m_AdapterDesc);")){
        return false;
    }
    if (m_AdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
        // WARP device will fail.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "WRAP Device not supported: %x",
                     m_hr);
        return false;
    }
    
    // A dedicated GPU (dGPU) doesn’t need shared memory, generally less than 512 MB, we can set a max limit at 2 GB.
    // Conversely, an integrated GPU (iGPU) relies mostly on shared memory, generally more than 2 GB, we can set a min limit at 512 MB.
    // We check that Shared memeory is more than 512 MB and DedicatedMemory less than 2 GB
    if(m_AdapterDesc.SharedSystemMemory >= (512 * 1024 * 1024) && m_AdapterDesc.DedicatedVideoMemory <= (2048ull * 1024 * 1024)){
        m_IsIntegratedGPU = true;
    }
    
    // Check if on battery
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        m_IsOnBattery = status.ACLineStatus == 0; // 0 = on battery, 1 = plugged in
    }
    
    // Low-end GPU are less than 4GB
    if(m_AdapterDesc.DedicatedVideoMemory <= (4096ull * 1024 * 1024)){
        m_IsLowEndGPU = true;
    }

    m_VideoEnhancement->setAdapterIndex(m_AdapterIndex);
    m_VideoEnhancement->setVendorID(m_AdapterDesc.VendorId);
    m_VideoEnhancement->setIntegratedGPU(m_IsIntegratedGPU);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Detected GPU %d: %S (%x:%x)",
                m_AdapterIndex,
                m_AdapterDesc.Description,
                m_AdapterDesc.VendorId,
                m_AdapterDesc.DeviceId);

    return true;
}

/**
 * \brief Timer checkpoint
 *
 * Trace the code with comment and time
 *
 * \return void
 */
void D3D12VARenderer::TimerInfo(const char* comment, bool start)
{
#ifdef QT_DEBUG
    if(!m_TimerInfo)
        return;
    
    qInfo() << "Timer Info: " << QString::number(m_Timer.nsecsElapsed() / 1'000'000.0, 'f', 3).toUtf8().constData() << comment;
    
    if(start)
        m_Timer.start();
#endif
}

/**
 * \brief Check decoder support for the configured video format
 *
 * Query the D3D11 video device to verify hardware decoding capability
 * for the current video format, profile, and output pixel format.
 *
 * \return bool True if hardware decoding is supported, false otherwise
 */
bool D3D12VARenderer::checkDecoderDX11Support()
{
    struct {
        GUID profile;
        DXGI_FORMAT format;
        const char* name;
    } decodeConfig = {};

    switch (m_DecoderParams.videoFormat)
    {
    case VIDEO_FORMAT_H264:
        decodeConfig = {
            D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
            DXGI_FORMAT_NV12,
            "H.264"
        };
        break;

    case VIDEO_FORMAT_H264_HIGH8_444:
        // Unsupported by DXVA
        return false;

    case VIDEO_FORMAT_H265:
        decodeConfig = {
            D3D11_DECODER_PROFILE_HEVC_VLD_MAIN,
            DXGI_FORMAT_NV12,
            "HEVC Main"
        };
        break;

    case VIDEO_FORMAT_H265_MAIN10:
        decodeConfig = {
            D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10,
            DXGI_FORMAT_P010,
            "HEVC Main10"
        };
        break;

    case VIDEO_FORMAT_H265_REXT8_444:
        decodeConfig = {
            k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN_444,
            DXGI_FORMAT_AYUV,
            "HEVC Main 444 8-bit"
        };
        break;

    case VIDEO_FORMAT_H265_REXT10_444:
        decodeConfig = {
            k_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10_444,
            DXGI_FORMAT_Y410,
            "HEVC Main 444 10-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_MAIN8:
        decodeConfig = {
            D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0,
            DXGI_FORMAT_NV12,
            "AV1 Profile0"
        };
        break;

    case VIDEO_FORMAT_AV1_MAIN10:
        decodeConfig = {
            D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0,
            DXGI_FORMAT_P010,
            "AV1 Main 10-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_HIGH8_444:
        decodeConfig = {
            D3D11_DECODER_PROFILE_AV1_VLD_PROFILE1,
            DXGI_FORMAT_AYUV,
            "AV1 High 444 8-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_HIGH10_444:
        decodeConfig = {
            D3D11_DECODER_PROFILE_AV1_VLD_PROFILE1,
            DXGI_FORMAT_Y410,
            "AV1 High 444 10-bit"
        };
        break;

    default:
        SDL_assert(false);
        return false;
    }

    BOOL supported = FALSE;
    m_hr = m_D3D11VideoDevice->CheckVideoDecoderFormat(&decodeConfig.profile,
                                                       decodeConfig.format,
                                                       &supported);

    if (!verifyHResult(m_hr, "m_D3D11VideoDevice->CheckVideoDecoderFormat();")) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GPU doesn't support %s decoding: %x",
                     decodeConfig.name, m_hr);
        return false;
    }

    if (!supported) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GPU doesn't support %s decoding to target format",
                     decodeConfig.name);
        return false;
    }

    if (DXUtil::isFormatHybridDecodedByHardware(m_DecoderParams.videoFormat, m_AdapterDesc.VendorId, m_AdapterDesc.DeviceId)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPU decoding for format %x is blocked due to hardware limitations",
                    m_DecoderParams.videoFormat);
        return false;
    }

    return true;
}

/**
 * \brief Check decoder support for the configured video format
 *
 * Query the D3D12 video device to verify hardware decoding capability
 * for the current video format, profile, and output pixel format.
 *
 * \return bool True if hardware decoding is supported, false otherwise
 */
bool D3D12VARenderer::checkDecoderDX12Support()
{
    struct {
        GUID profile;
        DXGI_FORMAT format;
        const char* name;
    } decodeConfig = {};

    switch (m_DecoderParams.videoFormat)
    {
    case VIDEO_FORMAT_H264:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_H264,
            DXGI_FORMAT_NV12,
            "H.264"
        };
        break;

    case VIDEO_FORMAT_H264_HIGH8_444:
        // Unsupported by DXVA
        return false;

    case VIDEO_FORMAT_H265:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN,
            DXGI_FORMAT_NV12,
            "HEVC Main"
        };
        break;

    case VIDEO_FORMAT_H265_MAIN10:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10,
            DXGI_FORMAT_P010,
            "HEVC Main10"
        };
        break;

    case VIDEO_FORMAT_H265_REXT8_444:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN_444,
            DXGI_FORMAT_AYUV,
            "HEVC Main 444 8-bit"
        };
        break;

    case VIDEO_FORMAT_H265_REXT10_444:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_HEVC_MAIN10_444,
            DXGI_FORMAT_Y410,
            "HEVC Main 444 10-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_MAIN8:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0,
            DXGI_FORMAT_NV12,
            "AV1 Profile0"
        };
        break;

    case VIDEO_FORMAT_AV1_MAIN10:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE0,
            DXGI_FORMAT_P010,
            "AV1 Main 10-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_HIGH8_444:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE1,
            DXGI_FORMAT_AYUV,
            "AV1 High 444 8-bit"
        };
        break;

    case VIDEO_FORMAT_AV1_HIGH10_444:
        decodeConfig = {
            D3D12_VIDEO_DECODE_PROFILE_AV1_PROFILE1,
            DXGI_FORMAT_Y410,
            "AV1 High 444 10-bit"
        };
        break;

    default:
        SDL_assert(false);
        return false;
    }

    D3D12_VIDEO_DECODE_CONFIGURATION config = {};
    config.DecodeProfile        = decodeConfig.profile;
    config.BitstreamEncryption  = D3D12_BITSTREAM_ENCRYPTION_TYPE_NONE;
    config.InterlaceType        = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
    
    DXGI_RATIONAL frameRate = {};
    frameRate.Numerator   = static_cast<UINT>(m_DecoderParams.frameRate);
    frameRate.Denominator = 1;
    
    D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT decodeSupport = {};
    decodeSupport.NodeIndex        = 0;
    decodeSupport.Configuration    = config;
    decodeSupport.Width            = m_DecoderParams.width;
    decodeSupport.Height           = m_DecoderParams.height;
    decodeSupport.DecodeFormat     = decodeConfig.format;
    decodeSupport.FrameRate        = frameRate;
    decodeSupport.BitRate          = 0;

    m_hr = m_VideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT,
                                          &decodeSupport,
                                          sizeof(decodeSupport));

    if(!verifyHResult(m_hr, "m_VideoDevice->CheckFeatureSupport(D3D12_FEATURE_VIDEO_DECODE_SUPPORT...);")){
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GPU doesn't support %s decoding: %x",
                     decodeConfig.name, m_hr);
        return false;
    }

    if (!(decodeSupport.SupportFlags & D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "GPU doesn't support %s decoding to target format",
                     decodeConfig.name);
        return false;
    }

    if (DXUtil::isFormatHybridDecodedByHardware(m_DecoderParams.videoFormat, m_AdapterDesc.VendorId, m_AdapterDesc.DeviceId)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "GPU decoding for format %x is blocked due to hardware limitations",
                    m_DecoderParams.videoFormat);
        return false;
    }

    return true;
}

/**
 * \brief Check Decoder type acceptance
 *
 * Due to some GPU and driver limitation, FFmpeg may not work properly.
 *
 * \return bool, Return true if accepted
 */
bool D3D12VARenderer::checkDecoderType()
{
    // FFmpeg decoder in yuv 4:4:4 only works with D3D11
    if (m_IsTextureYUV444) {
        return m_IsFrameD3D11;
    }
    
    // For H264 and AV1, AMD is more stable for decoding with FFmpeg in D3D11
    if (m_VideoEnhancement->isVendorAMD()) {
        // Force FFmpeg decoder on D3D11VA, D3D12VA is unstable.
        // H.264 and AV1 are freezing and stuttering due to reference frame issues:
        // https://ffmpeg.org/pipermail/ffmpeg-devel/2025-February/340089.html
        // HEVC is better, but still has issues with reference frame (can see an heavy block effect at window login page).
        return m_IsFrameD3D11;
    }
    
    // IntelVPL can work with D3D12, but I have issue with copying NV12 resource,
    // therefore I use D3D11, no additional latency impact
    if (m_VideoEnhancement->isVendorIntel()) {
        if (m_VendorVSRenabled)
            return m_IsFrameD3D11;
    }
    
    // By default, only accept D3D12va
    return !m_IsFrameD3D11;
}

/**
 * \brief Initialization
 *
 * Initialization of the renderer.
 *
 * \param PDECODER_PARAMETERS params
 * \return bool, Returns true if success
 */
bool D3D12VARenderer::initialize(PDECODER_PARAMETERS params)
{    
    // Variables initalization
    {
        m_DecoderParams = *params;

        // D3D12VideoProcessor only supports hardware decoding
        if (m_DecoderParams.vds == StreamingPreferences::VDS_FORCE_SOFTWARE) {
            return false;
        }

        // Check if HDR is enabled by the user in the UI settings.
        m_IsTexture10bits = m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_10BIT;
        
        // NOTE: We ignore the HDR setting as we now have a dynamic way to adapt the stream SDR/HDR<->SDR/HDR
        // Check if the Client display has HDR activated
        D3D12VARenderer::m_IsClientHDR = getDisplayHDRStatus();

        // Check if the input stream is YUV 4:4:4
        m_IsTextureYUV444 = m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_YUV444;
        
        // For stability reasons (like FFmpeg for AMD GPU, or YUV 4:4:4) we need to receive a D3D11 frame.
        // Some textures will be shared between the device D3D12 and D3D11.
        m_IsFrameD3D11 = m_VideoEnhancement->getDeviceType() == AV_HWDEVICE_TYPE_D3D11VA;
        
        // For vSync, we don't use m_DecoderParams.enableVsync
        m_Vsync = m_Preferences->enableVsync;
        
        // Setup the display resolution and frequency of the display,
        // It overwrite the behavior of Session::updateOptimalWindowDisplayMode() as for fullscreen exclusive mode,
        // we need to allow a higher resolution (with a different frequency thus) to leverage upscaling capability.
        if (!m_DecoderParams.testOnly) {
            
            do {
                
                bool fullscreenExclusive = false;
                bool fullscreenBorderless = false;
                
                switch (m_Preferences->windowMode) {
                case StreamingPreferences::WM_FULLSCREEN:
                    fullscreenExclusive = true;
                    break;
                case StreamingPreferences::WM_FULLSCREEN_DESKTOP:
                    fullscreenBorderless = true;
                    break;
                default:
                    break;
                }
                                
                if ((SDL_GetWindowFlags(m_DecoderParams.window) & SDL_WINDOW_FULLSCREEN_DESKTOP) == SDL_WINDOW_FULLSCREEN) {
                    // Only print when the window is actually in full-screen exclusive mode,
                    // otherwise we're not actually using the mode we've set here
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                                "Chosen best display mode: %dx%dx%d");
                }
                
                DISPLAY_DEVICEW m_DisplayDevice = {};
                m_DisplayDevice.cb = sizeof(m_DisplayDevice);
                
                DEVMODEW m_DevMode = {};
                m_DevMode.dmSize = sizeof(m_DevMode);
                
                DEVMODEW dmTemp = {};
                dmTemp.dmSize = sizeof(dmTemp);
                
                // Get the display where the application is running
                int displayIndex = SDL_GetWindowDisplayIndex(m_DecoderParams.window);
                if (displayIndex < 0)
                    break;
                
                // Get the position of the display
                SDL_Rect sdlBounds = {};
                if (SDL_GetDisplayBounds(displayIndex, &sdlBounds) != 0)
                    break;
                
                // The desktop setup
                SDL_DisplayMode desktopMode;
                if (SDL_GetDesktopDisplayMode(displayIndex, &desktopMode) != 0)
                    break;
                
                // The targeted ressolution and frequency
                SDL_DisplayMode targetMode = desktopMode;
                
                bool displayFound = false;
                
                // Find the corresponding Window device
                DWORD index = 0;
                while (EnumDisplayDevicesW(nullptr, index, &m_DisplayDevice, 0)) {
                    if (m_DisplayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE) {
                        
                        if (!EnumDisplaySettingsExW(m_DisplayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &dmTemp, 0))
                            continue;
                        
                        // Confirm this is the right display by comparing its position
                        if (dmTemp.dmPosition.x == sdlBounds.x && dmTemp.dmPosition.y == sdlBounds.y) {
                            displayFound = true;
                            m_DevMode = dmTemp;
                            break;
                        }
                        
                    }
                    index++;
                }
                
                // No display device found
                if (!displayFound)
                    break;
                
                // Find Frequency configurations
                index = 0;
                // Get highest frequency at the targeted resolution
                // It will be used in vSync and "fake" exclusive mode to take advantage of VRR capability. 
                int highestFrameRate = 0;
                while (EnumDisplaySettingsExW(m_DisplayDevice.DeviceName, index, &dmTemp, 0)) {
                    // Only check setups with the same resolution
                    if (dmTemp.dmPelsWidth == targetMode.w && dmTemp.dmPelsHeight == targetMode.h) {
                        
                        // Highest
                        if (dmTemp.dmDisplayFrequency > highestFrameRate) {
                            highestFrameRate = dmTemp.dmDisplayFrequency;
                        }
                        
                    }
                    index++;
                }
                
                // No frequency found
                if (highestFrameRate == 0)
                    break;
                
                if (fullscreenExclusive || m_Vsync)
                    targetMode.refresh_rate = highestFrameRate;
                
                // NOTE:
                // Windowed + Borderless: Keep display frequency
                // Windowed + Borderless + vSync: Set closest Set highest frequency (to get use of VRR)
                // Exclusive: Set highest frequency (to reduce tearing)
                // Exclusive + vSync: Set highest frequency (to get use of VRR)
                
                // We update only if the target is different as the initial setup.
                // For excluse, we force the update to enable VRR
                if (targetMode.refresh_rate != m_DevMode.dmDisplayFrequency) {
                    
                    // By default, do not change anything
                    m_DevMode.dmFields = 0;
                    
                    // Update the frequency only when vSync is enabled or in fullscreen mode
                    if (fullscreenExclusive || m_Vsync) {
                        if (targetMode.refresh_rate != m_DevMode.dmDisplayFrequency) {
                            m_DevMode.dmFields |= DM_DISPLAYFREQUENCY;
                            m_DevMode.dmDisplayFrequency = highestFrameRate;
                        }
                    }
                    
                    // We skip if we do no change
                    if (m_DevMode.dmFields == 0)
                        break;
                    
                    // Do not use exclusive CDS_FULLSCREEN as it doesn't work with VRR.
                    // VRR is important due to the non-fixed streaming frame rate.
                    LONG result = ChangeDisplaySettingsExW(
                        m_DisplayDevice.DeviceName,
                        &m_DevMode,
                        NULL,
                        0, // Change globally
                        NULL
                        );
                    
                    if (result != DISP_CHANGE_SUCCESSFUL)
                        break;
                    
                    // Switch to fullscreen
                    if (fullscreenExclusive || fullscreenBorderless) {
                        SDL_SetWindowDisplayMode(m_DecoderParams.window, &targetMode);
                        if (SDL_SetWindowFullscreen(m_DecoderParams.window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
                            break;
                    } else {
                        if (SDL_SetWindowFullscreen(m_DecoderParams.window, 0) != 0)
                            break;
                    }
                    
                    m_DecoderParams.frameRate = targetMode.refresh_rate;
                    
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Display setup: %dx%d @ %dHz",
                                m_DevMode.dmPelsWidth, m_DevMode.dmPelsHeight, m_DevMode.dmDisplayFrequency);
                }
                
            } while (false); // Run only once
            
        }

        // Calculate and fill the allocation sizes using the bitwise "round up to even" method.
        m_DecoderParams.textureWidth = (m_DecoderParams.width + 1) & ~1;
        m_DecoderParams.textureHeight = (m_DecoderParams.height + 1) & ~1;

        // Surfaces must be 16 pixel aligned for H.264 and 128 pixel aligned for everything else
        // https://github.com/FFmpeg/FFmpeg/blob/a234e5cd80224c95a205c1f3e297d8c04a1374c3/libavcodec/dxva2.c#L609-L616
        m_TextureAlignment = (m_DecoderParams.videoFormat & VIDEO_FORMAT_MASK_H264) ? 16 : 128;

        m_FrameWidth = FFALIGN(m_DecoderParams.textureWidth, m_TextureAlignment);
        m_FrameHeight = FFALIGN(m_DecoderParams.textureHeight, m_TextureAlignment);

        // Decoder information
        m_Decoder = {};
        m_Decoder.ColorRange = getDecoderColorRange();
        if (m_IsTexture10bits) {
            m_Decoder.Format = m_IsTextureYUV444 ? DXGI_FORMAT_Y410 : DXGI_FORMAT_P010;
            m_Decoder.AVFormat = m_IsTextureYUV444 ? AV_PIX_FMT_XV30 : AV_PIX_FMT_P010;
        } else {
            m_Decoder.Format = m_IsTextureYUV444 ? DXGI_FORMAT_AYUV : DXGI_FORMAT_NV12;
            m_Decoder.AVFormat = m_IsTextureYUV444 ? AV_PIX_FMT_VUYX : AV_PIX_FMT_NV12;
        }
        if (D3D12VARenderer::m_IsFrameHDR) {
            m_Decoder.ColorSpace = (getDecoderColorRange() & COLOR_RANGE_FULL) ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020;
        } else {
            m_Decoder.ColorSpace = (getDecoderColorRange() & COLOR_RANGE_FULL) ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
        }
    }

    // Dimensions initialization
    {
        // Used to crop the frame texture receive as the frame need to be aligned by blocks for performance purpose
        m_D3D11SrcBox.left = 0;
        m_D3D11SrcBox.top = 0;
        m_D3D11SrcBox.right = m_DecoderParams.textureWidth;
        m_D3D11SrcBox.bottom = m_DecoderParams.textureHeight;
        m_D3D11SrcBox.front = 0;
        m_D3D11SrcBox.back = 1;
        
        m_SrcBox.left = 0;
        m_SrcBox.top = 0;
        m_SrcBox.right = m_DecoderParams.textureWidth;
        m_SrcBox.bottom = m_DecoderParams.textureHeight;
        m_SrcBox.front = 0;
        m_SrcBox.back = 1;

        // Use the current window size as the swapchain size
        SDL_GetWindowSize(m_DecoderParams.window, (int*)&m_DisplayWidth, (int*)&m_DisplayHeight);

        // Rounddown to even number to avoid a crash at texture creation
        // If the window is odd in a driection, it will crop 1px the backbuffer in that direction
        m_DisplayWidth = (m_DisplayWidth + 1) & ~1;
        m_DisplayHeight = (m_DisplayHeight + 1) & ~1;

        // As m_Display corresponds to the application window, which may not have the same ratio as the Frame,
        // we calculate the size of the final texture to fit in the window without distortion
        m_OutputTextureInfo.width = m_DisplayWidth;
        m_OutputTextureInfo.height = m_DisplayHeight;
        m_OutputTextureInfo.left = 0;
        m_OutputTextureInfo.top = 0;

        // Scale the source to the destination surface while keeping the same ratio
        float ratioWidth = static_cast<float>(m_OutputTextureInfo.width) / static_cast<float>(m_DecoderParams.textureWidth);
        float ratioHeight = static_cast<float>(m_OutputTextureInfo.height) / static_cast<float>(m_DecoderParams.textureHeight);

        if (ratioHeight < ratioWidth) {
            // Adjust the Width
            m_OutputTextureInfo.width = static_cast<int>(std::floor(m_DecoderParams.textureWidth * ratioHeight));
            m_OutputTextureInfo.width = m_OutputTextureInfo.width & ~1;
            m_OutputTextureInfo.left = static_cast<int>(std::floor(  abs(m_DisplayWidth - m_OutputTextureInfo.width) / 2  ));
            m_OutputTextureInfo.left = m_OutputTextureInfo.left & ~1;
        } else if (ratioWidth < ratioHeight) {
            // Adjust the Height
            m_OutputTextureInfo.height = static_cast<int>(std::floor(m_DecoderParams.textureHeight * ratioWidth));
            m_OutputTextureInfo.height = m_OutputTextureInfo.height & ~1;
            m_OutputTextureInfo.top = static_cast<int>(std::floor(  abs(m_DisplayHeight - m_OutputTextureInfo.height) / 2  ));
            m_OutputTextureInfo.top = m_OutputTextureInfo.top & ~1;
        }

        m_D3D11OutputBox.left = 0;
        m_D3D11OutputBox.top = 0;
        m_D3D11OutputBox.front = 0;
        m_D3D11OutputBox.right = m_OutputTextureInfo.width;
        m_D3D11OutputBox.bottom = m_OutputTextureInfo.height;
        m_D3D11OutputBox.back = 1;
        
        m_OutputBox.left = 0;
        m_OutputBox.top = 0;
        m_OutputBox.front = 0;
        m_OutputBox.right = m_OutputTextureInfo.width;
        m_OutputBox.bottom = m_OutputTextureInfo.height;
        m_OutputBox.back = 1;
    }

    UINT dxgiFactoryFlags = 0;
#ifdef QT_DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    m_hr = CreateDXGIFactory2(dxgiFactoryFlags, __uuidof(IDXGIFactory6), (void**)m_Factory.GetAddressOf());
    if(!verifyHResult(m_hr, "CreateDXGIFactory2(dxgiFactoryFlags, __uuidof(IDXGIFactory6), (void**)m_Factory.GetAddressOf());")){
        return false;
    }

    if (!initialiazeAdapterInformation()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "initialiazeAdapterInformation() failed");
        return false;
    }

    // Device creation
    m_hr = D3D12CreateDevice(
        m_Adapter.Get(),
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_Device)
        );
    if(!verifyHResult(m_hr, "D3D12CreateDevice(... m_Device)")){
        return false;
    }

    // VideoDevice creation
    m_hr = m_Device.As(&m_VideoDevice);
    if(!verifyHResult(m_hr, "m_Device.As(&m_VideoDevice);")){
        return false;
    }

    if (qgetenv("D3D12VA_ENABLED") == "0") {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "D3D12VA is disabled by environment variable");
        return false;
    } else if (!IsWindows10OrGreater()) {
        // Use DXVA2 on anything older than Win10, so we don't have to handle a bunch
        // of legacy Win7/Win8 codepaths in here.
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "D3D12VA renderer is only supported on Windows 10 or later.");
        return false;
    }

#ifdef QT_DEBUG
    // Debug
    {
        // Provide more information, but could slow down the run
        if (m_DebugVerbose) {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                // Note: EnableDebugLayer makes the code less permissive and could fail in DEBUG mode while it works in Production mode.
                // WARNING: It easily bugs the application (like VideoProcessCommandList which can't be closed) and returns missleading information.
                debugController->EnableDebugLayer();
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "D3D12 Debug Layer is enabled.");
            }

            ComPtr<ID3D12Debug1> debugControllerGPU;
            if (SUCCEEDED(debugController.As(&debugControllerGPU))) {
                debugControllerGPU->SetEnableGPUBasedValidation(TRUE);
            }

            ComPtr<ID3D12DebugDevice2> debugDevice;
            if (SUCCEEDED(m_Device.As(&debugDevice))) {
                D3D12_DEBUG_FEATURE mask =
                    D3D12_DEBUG_FEATURE_ALLOW_BEHAVIOR_CHANGING_DEBUG_AIDS |
                    D3D12_DEBUG_FEATURE_CONSERVATIVE_RESOURCE_STATE_TRACKING;
                debugDevice->SetDebugParameter(
                    D3D12_DEBUG_DEVICE_PARAMETER_FEATURE_FLAGS,
                    &mask,
                    sizeof(mask)
                    );
            }

            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(m_Device.As(&infoQueue)))
            {
                // Stop the code on Error
                // infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
                // infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);

                // Do not display INFO messages
                D3D12_MESSAGE_SEVERITY severities[] = {
                    D3D12_MESSAGE_SEVERITY_INFO
                };
                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.NumSeverities = _countof(severities);
                filter.DenyList.pSeverityList = severities;
                infoQueue->PushStorageFilter(&filter);
            }
        }
    }
#endif
    
    // Device D3D11 initialization
    // We need to instantiate it as it can be used to check some features availability (Intel VPL)
    {
        D3D_FEATURE_LEVEL supportedFeatureLevels[] = { D3D_FEATURE_LEVEL_11_1 };
        
        m_D3D11Device.Reset();
        m_D3D11DeviceContext.Reset();
        
        ComPtr<ID3D11Device> D3D11Device;
        ComPtr<ID3D11DeviceContext> D3D11DeviceContext;
        
        D3D_FEATURE_LEVEL featureLevel;
        m_hr = D3D11CreateDevice(m_Adapter.Get(),
                                 D3D_DRIVER_TYPE_UNKNOWN,
                                 nullptr,
                                 D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                                 supportedFeatureLevels,
                                 ARRAYSIZE(supportedFeatureLevels),
                                 D3D11_SDK_VERSION,
                                 D3D11Device.GetAddressOf(),
                                 &featureLevel,
                                 D3D11DeviceContext.GetAddressOf());
        if(!verifyHResult(m_hr, "D3D11CreateDevice(... D3D11Device)")){
            return false;
        }
        D3D11Device.As(&m_D3D11Device);
        D3D11DeviceContext.As(&m_D3D11DeviceContext);
        
        // DX11 Device creation
        m_hr = D3D11Device.As(&m_D3D11VideoDevice);
        if(!verifyHResult(m_hr, "D3D11Device.As(&m_D3D11VideoDevice);")){
            return false;
        }
        
        // DX11 VideoDevice creation
        m_hr = D3D11Device.As(&m_D3D11VideoDevice);
        if(!verifyHResult(m_hr, "D3D11Device.As(&m_D3D11VideoDevice);")){
            return false;
        }
        
        // DX11 DeviceContext creation
        m_hr = D3D11DeviceContext.As(&m_D3D11DeviceContext);
        if(!verifyHResult(m_hr, "D3D11DeviceContext.As(&m_D3D11DeviceContext);")){
            return false;
        }
    }
    
    // Check the codec support
    {
        if (m_IsFrameD3D11) {
            if (!checkDecoderDX11Support()){
                return false;
            }
        } else {
            if (!checkDecoderDX12Support()){
                return false;
            }
        }
    }
    
    // Command allocator and command list initialization
    {
        // PROCESS
        m_hr = m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS,
            IID_PPV_ARGS(&m_VideoProcessCommandAllocator)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandAllocator(... m_VideoProcessCommandAllocator)")){
            return false;
        }
        
        m_hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS,
            m_VideoProcessCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_VideoProcessCommandList)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandList(... m_VideoProcessCommandList)")){
            return false;
        }
        
        // Command lists are created in recording state. Close for now.
        m_hr = m_VideoProcessCommandList->Close();
        if(!verifyHResult(m_hr, "m_VideoProcessCommandList->Close();")){
            return false;
        }
        
        D3D12_COMMAND_QUEUE_DESC queueProcessDesc = {};
        queueProcessDesc.Type = D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS;
        queueProcessDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        queueProcessDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueProcessDesc.NodeMask = 0;
        m_hr = m_Device->CreateCommandQueue(&queueProcessDesc, IID_PPV_ARGS(&m_VideoProcessCommandQueue));
        if(!verifyHResult(m_hr, "m_Device->CreateCommandQueue(&queueProcessDesc, IID_PPV_ARGS(&m_VideoProcessCommandQueue));")){
            return false;
        }
        
        // GRAPHICS
        m_hr = m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_GraphicsCommandAllocator)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandAllocator(... m_GraphicsCommandAllocator)")){
            return false;
        }
        
        m_hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_GraphicsCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_GraphicsCommandList)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandList(... m_GraphicsCommandList)")){
            return false;
        }
        
        // Command lists are created in recording state. Close for now.
        m_hr = m_GraphicsCommandList->Close();
        if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
            return false;
        }
        
        D3D12_COMMAND_QUEUE_DESC queueGraphicsDesc = {};
        queueGraphicsDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueGraphicsDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
        queueGraphicsDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueGraphicsDesc.NodeMask = 0;
        m_hr = m_Device->CreateCommandQueue(&queueGraphicsDesc, IID_PPV_ARGS(&m_GraphicsCommandQueue));
        if(!verifyHResult(m_hr, "m_Device->CreateCommandQueue(&queueGraphicsDesc, IID_PPV_ARGS(&m_GraphicsCommandQueue));")){
            return false;
        }
        
        // OVERLAY
        m_hr = m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_OverlayCommandAllocator)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandAllocator(... m_OverlayCommandAllocator)")){
            return false;
        }
        
        m_hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_OverlayCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_OverlayCommandList)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandList(... m_OverlayCommandList)")){
            return false;
        }
        
        // Command lists are created in recording state. Close for now.
        m_hr = m_OverlayCommandList->Close();
        if(!verifyHResult(m_hr, "m_OverlayCommandList->Close();")){
            return false;
        }
        
        D3D12_COMMAND_QUEUE_DESC queueOverlayDesc = {};
        queueOverlayDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueOverlayDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueOverlayDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueOverlayDesc.NodeMask = 0;
        m_hr = m_Device->CreateCommandQueue(&queueOverlayDesc, IID_PPV_ARGS(&m_OverlayCommandQueue));
        if(!verifyHResult(m_hr, "m_Device->CreateCommandQueue(&queueOverlayDesc, IID_PPV_ARGS(&m_OverlayCommandQueue));")){
            return false;
        }
        
        // PICTURE
        m_hr = m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_PictureCommandAllocator)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandAllocator(... m_PictureCommandAllocator)")){
            return false;
        }
        
        m_hr = m_Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            m_PictureCommandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_PictureCommandList)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommandList(... m_PictureCommandList)")){
            return false;
        }
        
        // Command lists are created in recording state. Close for now.
        m_hr = m_PictureCommandList->Close();
        if(!verifyHResult(m_hr, "m_PictureCommandList->Close();")){
            return false;
        }
        
        D3D12_COMMAND_QUEUE_DESC queuePictureDesc = {};
        queuePictureDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queuePictureDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queuePictureDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queuePictureDesc.NodeMask = 0;
        m_hr = m_Device->CreateCommandQueue(&queuePictureDesc, IID_PPV_ARGS(&m_PictureCommandQueue));
        if(!verifyHResult(m_hr, "m_Device->CreateCommandQueue(&queuePictureDesc, IID_PPV_ARGS(&m_PictureCommandQueue));")){
            return false;
        }
    }
    
    // YUV Textures initialization
    {
        // Texture to receive the frame texture
        CD3DX12_HEAP_PROPERTIES heapFrameProps(D3D12_HEAP_TYPE_DEFAULT);
        
        D3D12_RESOURCE_DESC descFrame = {};
        descFrame.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        descFrame.Alignment = 0;
        descFrame.Width = m_FrameWidth;
        descFrame.Height = m_FrameHeight;
        descFrame.DepthOrArraySize = 1;
        descFrame.MipLevels = 1;
        descFrame.Format = m_Decoder.Format;
        descFrame.SampleDesc.Count = 1;
        descFrame.SampleDesc.Quality = 0;
        descFrame.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        descFrame.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if (m_IsFrameD3D11)
            descFrame.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        m_hr = m_Device->CreateCommittedResource(
            &heapFrameProps,
            m_IsFrameD3D11 ? D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE,
            &descFrame,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_FrameTexture)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_FrameTexture)")){
            return false;
        }
        
        // m_D3D11FrameTexture
        if (m_IsFrameD3D11) {
            HANDLE sharedHandle1 = nullptr;
            m_Device->CreateSharedHandle(
                        m_FrameTexture.Get(),
                        nullptr,
                        GENERIC_ALL,
                        nullptr,
                        &sharedHandle1);
            
            m_hr = m_D3D11Device->OpenSharedResource1(sharedHandle1, IID_PPV_ARGS(m_D3D11FrameTexture.GetAddressOf()));
            if(!verifyHResult(m_hr, "OpenSharedResource1(... m_D3D11FrameTexture)")){
                return false;
            }
            
            CloseHandle(sharedHandle1);
        }
        
        // Upscaled YUV texture
        CD3DX12_HEAP_PROPERTIES heapYUVProps(D3D12_HEAP_TYPE_DEFAULT);
        
        D3D12_RESOURCE_DESC descYUV = {};
        descYUV.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        descYUV.Alignment = 0;
        descYUV.Width = m_OutputTextureInfo.width;
        descYUV.Height = m_OutputTextureInfo.height;
        descYUV.DepthOrArraySize = 1;
        descYUV.MipLevels = 1;
        descYUV.Format = m_Decoder.Format;
        descYUV.SampleDesc.Count = 1;
        descYUV.SampleDesc.Quality = 0;
        descYUV.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        descYUV.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (m_IsFrameD3D11)
            descYUV.Flags |= D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        m_hr = m_Device->CreateCommittedResource(
            &heapYUVProps,
            m_IsFrameD3D11 ? D3D12_HEAP_FLAG_SHARED : D3D12_HEAP_FLAG_NONE,
            &descYUV,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_YUVTextureUpscaled)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_YUVTextureUpscaled)")){
            return false;
        }
        
        // m_D3D11YUVTextureUpscaled
        if (m_IsFrameD3D11) {
            HANDLE sharedHandle1 = nullptr;
            m_Device->CreateSharedHandle(
                        m_YUVTextureUpscaled.Get(),
                        nullptr,
                        GENERIC_ALL,
                        nullptr,
                        &sharedHandle1);
            
            m_hr = m_D3D11Device->OpenSharedResource1(sharedHandle1, IID_PPV_ARGS(m_D3D11YUVTextureUpscaled.GetAddressOf()));
            if(!verifyHResult(m_hr, "OpenSharedResource1(... m_D3D11YUVTextureUpscaled)")){
                return false;
            }
            
            CloseHandle(sharedHandle1);
        }
    }

    // Set Converter and Upscaler according to the GPU
    enhanceAutoSelection();
    
    // Check if we use FFmpeg decoder in D3D12 or fallback to D3D11 in specifics cases    
    if(!checkDecoderType()){
        return false;
    }
    
    m_RGBFormat = (m_IsTexture10bits || m_VendorHDRenabled) ? DXGI_FORMAT_R10G10B10A2_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
    m_RGBColorSpace = (D3D12VARenderer::m_IsFrameHDR || m_VendorHDRenabled) ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;    
    
    // RGB Textures initialization
    {
        D3D12_CLEAR_VALUE clearValueRGB = {};
        clearValueRGB.Format = m_RGBFormat;
        clearValueRGB.Color[0] = 0.0f;
        clearValueRGB.Color[1] = 0.0f;
        clearValueRGB.Color[2] = 0.0f;
        clearValueRGB.Color[3] = 1.0f;
        
        D3D12_CLEAR_VALUE clearValueRGBoutput = {};
        clearValueRGBoutput.Format = m_RGBFormat;
        clearValueRGBoutput.Color[0] = 0.0f;
        clearValueRGBoutput.Color[1] = 0.0f;
        clearValueRGBoutput.Color[2] = 0.0f;
        clearValueRGBoutput.Color[3] = 1.0f;
        
        // Converted texture into RGB format
        CD3DX12_HEAP_PROPERTIES heapRGBProps(D3D12_HEAP_TYPE_DEFAULT);
        
        D3D12_RESOURCE_DESC descRGB = {};
        descRGB.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        descRGB.Alignment = 0;
        descRGB.Width = m_DecoderParams.textureWidth;
        descRGB.Height = m_DecoderParams.textureHeight;
        descRGB.DepthOrArraySize = 1;
        descRGB.MipLevels = 1;
        descRGB.Format = m_RGBFormat;
        descRGB.SampleDesc.Count = 1;
        descRGB.SampleDesc.Quality = 0;
        descRGB.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        descRGB.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        m_hr = m_Device->CreateCommittedResource(
            &heapRGBProps,
            D3D12_HEAP_FLAG_NONE,
            &descRGB,
            D3D12_RESOURCE_STATE_COMMON,
            &clearValueRGB,
            IID_PPV_ARGS(&m_RGBTexture)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_RGBTexture)")){
            return false;
        }
        
        // Upscaled RGB texture
        descRGB.Width = m_OutputTextureInfo.width;
        descRGB.Height = m_OutputTextureInfo.height;
        m_hr = m_Device->CreateCommittedResource(
            &heapRGBProps,
            D3D12_HEAP_FLAG_NONE,
            &descRGB,
            D3D12_RESOURCE_STATE_COMMON,
            &clearValueRGB,
            IID_PPV_ARGS(&m_RGBTextureUpscaled)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_RGBTextureUpscaled)")){
            return false;
        }
        
        // Texture processed and ready to be copied into the back buffer resource
        CD3DX12_HEAP_PROPERTIES heapOutputProps(D3D12_HEAP_TYPE_DEFAULT);
        
        D3D12_RESOURCE_DESC descOutput = {};
        descOutput.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        descOutput.Alignment = 0;
        descOutput.Width = m_OutputTextureInfo.width;
        descOutput.Height = m_OutputTextureInfo.height;
        descOutput.DepthOrArraySize = 1;
        descOutput.MipLevels = 1;
        descOutput.Format = m_RGBFormat;
        descOutput.SampleDesc.Count = 1;
        descOutput.SampleDesc.Quality = 0;
        descOutput.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        descOutput.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        m_hr = m_Device->CreateCommittedResource(
            &heapOutputProps,
            D3D12_HEAP_FLAG_NONE,
            &descOutput,
            D3D12_RESOURCE_STATE_COMMON,
            &clearValueRGBoutput,
            IID_PPV_ARGS(&m_OutputTexture)
            );
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_OutputTexture)")){
            return false;
        }
    }
    
    // m_D3D12Fence & m_D3D11Fence
    {
        m_hr = m_Device->CreateFence(
            0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_D3D12Fence));
        if(!verifyHResult(m_hr, "CreateFence(... m_D3D12Fence)")){
            return false;
        }
        
        HANDLE fenceSharedHandle = nullptr;
        m_hr = m_Device->CreateSharedHandle(
            m_D3D12Fence.Get(),
            nullptr,
            GENERIC_ALL,
            nullptr,
            &fenceSharedHandle);
        if(!verifyHResult(m_hr, "CreateFence(... m_D3D12Fence)")){
            return false;
        }
        
        m_hr = m_D3D11Device->OpenSharedFence(
            fenceSharedHandle,
            IID_PPV_ARGS(&m_D3D11Fence)
            );
        if(!verifyHResult(m_hr, "OpenSharedFence(... m_D3D11Fence)")){
            return false;
        }
        CloseHandle(fenceSharedHandle);
    }
    
    // IntelVPL textures
    if (m_RenderStep1 == RenderStep::UPSCALE_VPL && m_VppSession) {
        
        // Resize
        m_VppSurfaceIn.resize(m_VppReq[0].NumFrameSuggested);
        m_VppSurfaceOut.resize(m_VppReq[1].NumFrameSuggested);
        
        // Input VPL Texture
        D3D11_TEXTURE2D_DESC descIn11 = {};
        descIn11.Width = ALIGN16(m_DecoderParams.textureWidth);
        descIn11.Height = ALIGN16(m_DecoderParams.textureHeight);
        descIn11.MipLevels = 1;
        descIn11.ArraySize = 1;
        descIn11.Format = m_Decoder.Format;
        descIn11.SampleDesc.Quality = 0;
        descIn11.SampleDesc.Count = 1;
        descIn11.CPUAccessFlags = 0;
        descIn11.Usage = D3D11_USAGE_DEFAULT;
        descIn11.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        descIn11.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        
        CD3DX12_HEAP_PROPERTIES heapInProps(D3D12_HEAP_TYPE_DEFAULT);
        
        D3D12_RESOURCE_DESC descIn12 = {};
        descIn12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        descIn12.Alignment = 0;
        descIn12.Width = ALIGN16(m_DecoderParams.textureWidth);
        descIn12.Height = ALIGN16(m_DecoderParams.textureHeight);
        descIn12.DepthOrArraySize = 1;
        descIn12.MipLevels = 1;
        descIn12.Format = m_Decoder.Format;
        descIn12.SampleDesc.Count = 1;
        descIn12.SampleDesc.Quality = 0;
        descIn12.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        descIn12.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

        for (int i = 0; i < m_VppReq[0].NumFrameSuggested; i++) {
            if (m_IsFrameD3D11) {
                
                m_hr = m_D3D11Device->CreateTexture2D(&descIn11, nullptr, m_VppSurfaceIn[i].textureD3D11.GetAddressOf());
                if(!verifyHResult(m_hr, "m_D3D11Device->CreateTexture2D(... m_VppSurfaceIn[i].textureD3D11)")){
                    return false;
                }
                
                m_VppSurfaceIn[i].textureLinkD3D11.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_D3D11_TEX2D;
                m_VppSurfaceIn[i].textureLinkD3D11.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
                m_VppSurfaceIn[i].textureLinkD3D11.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceD3D11Tex2D );
                m_VppSurfaceIn[i].textureLinkD3D11.SurfaceInterface.Version.Major = 1;
                m_VppSurfaceIn[i].textureLinkD3D11.SurfaceInterface.Version.Minor = 0;
                m_VppSurfaceIn[i].textureLinkD3D11.texture2D = m_VppSurfaceIn[i].textureD3D11.Get();
                
            } else {
            
                m_hr = m_Device->CreateCommittedResource(
                            &heapInProps,
                            D3D12_HEAP_FLAG_SHARED,
                            &descIn12,
                            D3D12_RESOURCE_STATE_COMMON,
                            nullptr,
                            IID_PPV_ARGS(&m_VppSurfaceIn[i].textureD3D12)
                            );
                if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_VppSurfaceIn[i].textureD3D12)")){
                    return false;
                }
                
                m_VppSurfaceIn[i].textureLinkD3D12.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_D3D12_TEX2D;
                m_VppSurfaceIn[i].textureLinkD3D12.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
                m_VppSurfaceIn[i].textureLinkD3D12.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceD3D12Tex2D );
                m_VppSurfaceIn[i].textureLinkD3D12.SurfaceInterface.Version.Major = 1;
                m_VppSurfaceIn[i].textureLinkD3D12.SurfaceInterface.Version.Minor = 0;
                m_VppSurfaceIn[i].textureLinkD3D12.texture2D = m_VppSurfaceIn[i].textureD3D12.Get();
            
            }
        }
        
        // Output VPL Texture
        
        for (int i = 0; i < m_VppReq[1].NumFrameSuggested; i++) {
            if (m_IsFrameD3D11) {
                
                m_VppSurfaceOut[i].textureLinkD3D11.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_D3D11_TEX2D;
                m_VppSurfaceOut[i].textureLinkD3D11.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
                m_VppSurfaceOut[i].textureLinkD3D11.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceD3D11Tex2D);
                m_VppSurfaceOut[i].textureLinkD3D11.SurfaceInterface.Version.Major = 1;
                m_VppSurfaceOut[i].textureLinkD3D11.SurfaceInterface.Version.Minor = 0;
                m_VppSurfaceOut[i].textureLinkD3D11.texture2D = nullptr;
                
            } else {
                
                m_VppSurfaceOut[i].textureLinkD3D12.SurfaceInterface.Header.SurfaceType  = MFX_SURFACE_TYPE_D3D12_TEX2D;
                m_VppSurfaceOut[i].textureLinkD3D12.SurfaceInterface.Header.SurfaceFlags = MFX_SURFACE_FLAG_IMPORT_SHARED;
                m_VppSurfaceOut[i].textureLinkD3D12.SurfaceInterface.Header.StructSize   = sizeof(mfxSurfaceD3D12Tex2D);
                m_VppSurfaceOut[i].textureLinkD3D12.SurfaceInterface.Version.Major = 1;
                m_VppSurfaceOut[i].textureLinkD3D12.SurfaceInterface.Version.Minor = 0;
                m_VppSurfaceOut[i].textureLinkD3D12.texture2D = nullptr;
                
            }
        }
    }

    // SwapChain initialization
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width = m_DisplayWidth;
        swapChainDesc.Height = m_DisplayHeight;
        swapChainDesc.Format = m_RGBFormat;
        swapChainDesc.Stereo = FALSE;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = m_FrameCount;
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        // Use DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING with flip mode for non-vsync case, if possible.
        // NOTE: This is only possible in windowed or borderless windowed mode.
        if (!m_Vsync) {
            BOOL allowTearing = false;
            m_hr = m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                  &allowTearing,
                                                  sizeof(allowTearing));
            if (SUCCEEDED(m_hr)) {
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
                             m_hr);
            }
        }

        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        SDL_GetWindowWMInfo(m_DecoderParams.window, &info);
        SDL_assert(info.subsystem == SDL_SYSWM_WINDOWS);
        
        // swapChain
        ComPtr<IDXGISwapChain1> swapChain;
        m_hr = m_Factory->CreateSwapChainForHwnd(
            m_GraphicsCommandQueue.Get(),
            info.info.win.window,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain
            );
        if(!verifyHResult(m_hr, "m_Factory->CreateSwapChainForHwnd(... swapChain)")){
            return false;
        }

        m_hr = swapChain.As(&m_SwapChain);
        if(!verifyHResult(m_hr, "swapChain.As(&m_SwapChain);")){
            return false;
        }

        // Reduce latency
        m_FrameLatencyWaitableObject = m_SwapChain->GetFrameLatencyWaitableObject();
        m_SwapChain->SetMaximumFrameLatency(1);

        // SDR/HDR
        m_SwapChain->SetColorSpace1(m_RGBColorSpace);

        // Disable Alt+Enter, PrintScreen, and window message snooping. This makes
        // it safe to run the renderer on a separate rendering thread rather than
        // requiring the main (message loop) thread.
        m_hr = m_Factory->MakeWindowAssociation(info.info.win.window, DXGI_MWA_NO_WINDOW_CHANGES);
    }

    // Video Processor initialization
    {   
        D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1 inputStreamFiltersArgs = {};

        D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inputStreamConvert = {};
        inputStreamConvert.Format = m_Decoder.Format;
        inputStreamConvert.ColorSpace = m_Decoder.ColorSpace;
        inputStreamConvert.SourceAspectRatio = {1, 1};
        inputStreamConvert.DestinationAspectRatio = {1, 1};
        inputStreamConvert.FrameRate.Numerator = m_DecoderParams.frameRate;
        inputStreamConvert.FrameRate.Denominator = 1;
        inputStreamConvert.SourceSizeRange = {
            static_cast<UINT>(m_DecoderParams.textureWidth),
            static_cast<UINT>(m_DecoderParams.textureHeight),
            static_cast<UINT>(m_DecoderParams.textureWidth),
            static_cast<UINT>(m_DecoderParams.textureHeight)
        };
        inputStreamConvert.DestinationSizeRange = {
            static_cast<UINT>(m_DecoderParams.textureWidth),
            static_cast<UINT>(m_DecoderParams.textureHeight),
            static_cast<UINT>(m_DecoderParams.textureWidth),
            static_cast<UINT>(m_DecoderParams.textureHeight)
        };
        inputStreamConvert.EnableOrientation = FALSE;
        inputStreamConvert.FilterFlags = D3D12_VIDEO_PROCESS_FILTER_FLAG_NONE;
        inputStreamConvert.StereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
        inputStreamConvert.FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
        inputStreamConvert.DeinterlaceMode = D3D12_VIDEO_PROCESS_DEINTERLACE_FLAG_NONE;
        inputStreamConvert.EnableAlphaBlending = FALSE;
        inputStreamConvert.LumaKey = {0, 0};
        inputStreamConvert.NumPastFrames = 0;
        inputStreamConvert.NumFutureFrames = 0;
        inputStreamConvert.EnableAutoProcessing = FALSE;

        // Render Step 2
        D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inputStreamUpscaler = inputStreamConvert;
        inputStreamUpscaler.Format = m_RGBFormat;
        inputStreamUpscaler.ColorSpace = m_RGBColorSpace;
        inputStreamUpscaler.DestinationSizeRange = {
            static_cast<UINT>(m_OutputTextureInfo.width),
            static_cast<UINT>(m_OutputTextureInfo.height),
            static_cast<UINT>(m_OutputTextureInfo.width),
            static_cast<UINT>(m_OutputTextureInfo.height)
        };

        D3D12_VIDEO_PROCESS_INPUT_STREAM_DESC inputStreamUpscalerConvert = inputStreamConvert;
        inputStreamUpscalerConvert.DestinationSizeRange = {
            static_cast<UINT>(m_OutputTextureInfo.width),
            static_cast<UINT>(m_OutputTextureInfo.height),
            static_cast<UINT>(m_OutputTextureInfo.width),
            static_cast<UINT>(m_OutputTextureInfo.height)
        };
        
        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC outputStreamRGB = {};
        outputStreamRGB.Format = m_RGBFormat;
        outputStreamRGB.ColorSpace = m_RGBColorSpace;
        outputStreamRGB.AlphaFillMode = D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
        outputStreamRGB.AlphaFillModeSourceStreamIndex = 0;
        outputStreamRGB.BackgroundColor[0] = 0.0f;
        outputStreamRGB.BackgroundColor[1] = 0.0f;
        outputStreamRGB.BackgroundColor[2] = 0.0f;
        outputStreamRGB.BackgroundColor[3] = 1.0f;
        outputStreamRGB.FrameRate.Numerator = m_DecoderParams.frameRate;
        outputStreamRGB.FrameRate.Denominator = 1;
        outputStreamRGB.EnableStereo = FALSE;

        D3D12_VIDEO_PROCESS_OUTPUT_STREAM_DESC outputStreamYUV = {};
        outputStreamYUV.Format = m_Decoder.Format;
        outputStreamYUV.ColorSpace = m_Decoder.ColorSpace;
        outputStreamYUV.AlphaFillMode = D3D12_VIDEO_PROCESS_ALPHA_FILL_MODE_OPAQUE;
        outputStreamYUV.AlphaFillModeSourceStreamIndex = 0;
        outputStreamYUV.BackgroundColor[0] = 0.0f;
        outputStreamYUV.BackgroundColor[1] = 0.0f;
        outputStreamYUV.BackgroundColor[2] = 0.0f;
        outputStreamYUV.BackgroundColor[3] = 1.0f;
        outputStreamYUV.FrameRate.Numerator = m_DecoderParams.frameRate;
        outputStreamYUV.FrameRate.Denominator = 1;
        outputStreamYUV.EnableStereo = FALSE;

        // Check support
        struct KVFormat {
            DXGI_FORMAT key;
            std::string value;
        };
        struct KVCS {
            DXGI_COLOR_SPACE_TYPE key;
            std::string value;
        };

        // Texture formats
        std::unordered_map<DXGI_FORMAT, std::string> formats = {
            {DXGI_FORMAT_NV12, "DXGI_FORMAT_NV12"},
            {DXGI_FORMAT_P010, "DXGI_FORMAT_P010"},
            {DXGI_FORMAT_AYUV, "DXGI_FORMAT_AYUV"},
            {DXGI_FORMAT_Y410, "DXGI_FORMAT_Y410"},
            {DXGI_FORMAT_R8G8B8A8_UNORM, "DXGI_FORMAT_R8G8B8A8_UNORM"},
            {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB"},
            {DXGI_FORMAT_R10G10B10A2_UNORM, "DXGI_FORMAT_R10G10B10A2_UNORM"},
            {DXGI_FORMAT_R16G16B16A16_UNORM, "DXGI_FORMAT_R16G16B16A16_UNORM"}
        };

        // Color Spaces
        std::unordered_map<DXGI_COLOR_SPACE_TYPE, std::string> colorSpaces = {
            {DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601, "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601"},
            {DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709, "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709"},
            {DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020, "DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020"},
            {DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020, "DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020"},
            {DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601, "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601"},
            {DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709, "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709"},
            {DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020, "DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020"},
            {DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709"},
            {DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020, "DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020"},
            {DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020, "DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020"}
        };

        D3D12_FEATURE_DATA_VIDEO_PROCESS_SUPPORT supportVideoProcess = {};
        supportVideoProcess.NodeIndex = 0;
        supportVideoProcess.InputSample.Format.Format = m_Decoder.Format;
        supportVideoProcess.InputSample.Format.ColorSpace = m_Decoder.ColorSpace;
        supportVideoProcess.InputSample.Width = m_DecoderParams.textureWidth;
        supportVideoProcess.InputSample.Height = m_DecoderParams.textureHeight;
        supportVideoProcess.InputFieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
        supportVideoProcess.InputStereoFormat = D3D12_VIDEO_FRAME_STEREO_FORMAT_NONE;
        supportVideoProcess.InputFrameRate = { static_cast<UINT>(m_DecoderParams.frameRate), 1 };
        supportVideoProcess.OutputFormat.Format = m_RGBFormat;
        supportVideoProcess.OutputFormat.ColorSpace = m_RGBColorSpace;

        // Check if the conversion YUV -> RGB is supported
        if (SUCCEEDED(m_VideoDevice->CheckFeatureSupport(
                D3D12_FEATURE_VIDEO_PROCESS_SUPPORT,
                &supportVideoProcess,
                sizeof(supportVideoProcess)
                ))) {
            
            m_VideoProcessorConvertEnabled = true;
            
            qInfo().nospace().noquote()
                << "VideoProcessor conversion YUV->RGB supported: input=" << formats[supportVideoProcess.InputSample.Format.Format]
                << " (" << colorSpaces[supportVideoProcess.InputSample.Format.ColorSpace] << ")"
                << " -> output=" << formats[supportVideoProcess.OutputFormat.Format]
                << " (" << colorSpaces[supportVideoProcess.OutputFormat.ColorSpace] << ")";
        } else {
            qWarning().nospace().noquote()
                << "VideoProcessor conversion YUV->RGB not supported: input=" << formats[supportVideoProcess.InputSample.Format.Format]
                << " (" << colorSpaces[supportVideoProcess.InputSample.Format.ColorSpace] << ")"
                << " -> output=" << formats[supportVideoProcess.OutputFormat.Format]
                << " (" << colorSpaces[supportVideoProcess.OutputFormat.ColorSpace] << ")";
        }

        supportVideoProcess.OutputFormat.Format = m_Decoder.Format;
        supportVideoProcess.OutputFormat.ColorSpace = m_Decoder.ColorSpace;

        // Check if the Upscaler is supported
        if (SUCCEEDED(m_VideoDevice->CheckFeatureSupport(
                D3D12_FEATURE_VIDEO_PROCESS_SUPPORT,
                &supportVideoProcess,
                sizeof(supportVideoProcess)
                ))) {
            
            if(
                   supportVideoProcess.ScaleSupport.OutputSizeRange.MinWidth <= static_cast<UINT>(m_OutputTextureInfo.width)
                && supportVideoProcess.ScaleSupport.OutputSizeRange.MaxWidth >= static_cast<UINT>(m_OutputTextureInfo.width)
                && supportVideoProcess.ScaleSupport.OutputSizeRange.MinHeight <= static_cast<UINT>(m_OutputTextureInfo.height)
                && supportVideoProcess.ScaleSupport.OutputSizeRange.MaxHeight >= static_cast<UINT>(m_OutputTextureInfo.height)
                ) {
                m_VideoProcessorUpscalerEnabled = true;

                // Check is Auto Super Resolution is supported
                if(
                    supportVideoProcess.AutoProcessingSupport & D3D12_VIDEO_PROCESS_AUTO_PROCESSING_FLAG_SUPER_RESOLUTION
                    || supportVideoProcess.AutoProcessingSupport & D3D12_VIDEO_PROCESS_AUTO_PROCESSING_FLAG_EDGE_ENHANCEMENT
                    ){
                    inputStreamConvert.EnableAutoProcessing = TRUE;
                    inputStreamUpscaler.EnableAutoProcessing = TRUE;
                    inputStreamUpscalerConvert.EnableAutoProcessing = TRUE;
                }

                qInfo().nospace().noquote()
                    << "VideoProcessor upscaling supported: input=" << m_DecoderParams.textureWidth
                    << "x" << m_DecoderParams.textureHeight
                    << " -> output=" << m_OutputTextureInfo.width
                    << "x" << m_OutputTextureInfo.height;
            } else {
                qWarning().nospace().noquote()
                    << "VideoProcessor upscaling not supported: input=" << m_DecoderParams.textureWidth
                    << "x" << m_DecoderParams.textureHeight
                    << " -> output=" << m_OutputTextureInfo.width
                    << "x" << m_OutputTextureInfo.height;
            }

            D3D12_VIDEO_PROCESS_FILTER_RANGE range;
            float precent;

            if (!m_IsIntegratedGPU && supportVideoProcess.FilterSupport & D3D12_VIDEO_PROCESS_FILTER_FLAG_NOISE_REDUCTION) {
                // Apply 15% to Noise Reduction
                range = supportVideoProcess.FilterRangeSupport[D3D12_VIDEO_PROCESS_FILTER_NOISE_REDUCTION];
                precent = 0.15f;
                m_NoiseReductionValue = static_cast<INT32>(std::round((range.Minimum + (range.Maximum - range.Minimum) * precent)  / range.Multiplier)) * range.Multiplier;
                // Enable the filter at VideoProcessor level
                inputStreamUpscaler.FilterFlags |= D3D12_VIDEO_PROCESS_FILTER_FLAG_NOISE_REDUCTION;
                inputStreamUpscalerConvert.FilterFlags |= D3D12_VIDEO_PROCESS_FILTER_FLAG_NOISE_REDUCTION;
                qInfo() << "Noise Reduction Filter value: " << m_NoiseReductionValue << " [" << range.Minimum << "-" << range.Maximum << "]";
            }

            if (supportVideoProcess.FilterSupport & D3D12_VIDEO_PROCESS_FILTER_FLAG_EDGE_ENHANCEMENT) {
                // Apply 35% to Edge Enhancement
                range = supportVideoProcess.FilterRangeSupport[D3D12_VIDEO_PROCESS_FILTER_EDGE_ENHANCEMENT];
                precent = 0.35f;
                m_EdgeEnhancementValue = static_cast<INT32>(std::round((range.Minimum + (range.Maximum - range.Minimum) * precent)  / range.Multiplier)) * range.Multiplier;
                // Enable the filter at VideoProcessor level
                inputStreamUpscaler.FilterFlags |= D3D12_VIDEO_PROCESS_FILTER_FLAG_EDGE_ENHANCEMENT;
                inputStreamUpscalerConvert.FilterFlags |= D3D12_VIDEO_PROCESS_FILTER_FLAG_EDGE_ENHANCEMENT;
                qInfo() << "Edge Reduction Filter value: " << m_EdgeEnhancementValue << " [" << range.Minimum << "-" << range.Maximum << "]";
            }
        } else {
            qWarning().nospace().noquote()
                << "VideoProcessor upscaling not supported: input=" << m_DecoderParams.textureWidth
                << "x" << m_DecoderParams.textureHeight
                << " -> output=" << m_OutputTextureInfo.width
                << "x" << m_OutputTextureInfo.height;
        }
        
        m_InputArgsConvert.resize(m_FrameCount);
        m_OutputArgsConvert.resize(m_FrameCount);
        m_InputArgsUpscaler.resize(m_FrameCount);
        m_OutputArgsUpscaler.resize(m_FrameCount);
        m_InputArgsUpscalerConvert.resize(m_FrameCount);
        m_OutputArgsUpscalerConvert.resize(m_FrameCount);

        for (UINT n = 0; n < m_FrameCount; n++) {

            // CONVERT

            // Input
            m_InputArgsConvert[n].InputStream[0].pTexture2D = m_FrameTexture.Get();
            m_InputArgsConvert[n].InputStream[0].Subresource = 0;
            m_InputArgsConvert[n].InputStream[1].pTexture2D = nullptr;
            m_InputArgsConvert[n].InputStream[1].Subresource = 0;
            m_InputArgsConvert[n].Transform.Orientation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;
            m_InputArgsConvert[n].Transform.SourceRectangle = {
                0,
                0,
                static_cast<LONG>(m_DecoderParams.textureWidth),
                static_cast<LONG>(m_DecoderParams.textureHeight)
            };
            m_InputArgsConvert[n].Transform.DestinationRectangle = {
                0,
                0,
                static_cast<LONG>(m_DecoderParams.textureWidth),
                static_cast<LONG>(m_DecoderParams.textureHeight)
            };
            m_InputArgsConvert[n].Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
            m_InputArgsConvert[n].RateInfo.OutputIndex = 0;
            m_InputArgsConvert[n].RateInfo.InputFrameOrField = 0;
            m_InputArgsConvert[n].AlphaBlending.Enable = FALSE;
            m_InputArgsConvert[n].AlphaBlending.Alpha = 1.0f;
            m_InputArgsConvert[n].FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;

            // Output
            m_OutputArgsConvert[n].OutputStream[0].pTexture2D = m_RGBTexture.Get();
            m_OutputArgsConvert[n].OutputStream[0].Subresource = 0;
            m_OutputArgsConvert[n].OutputStream[1].pTexture2D = nullptr;
            m_OutputArgsConvert[n].OutputStream[1].Subresource = 0;
            m_OutputArgsConvert[n].TargetRectangle = {
                0,
                0,
                static_cast<LONG>(m_DecoderParams.textureWidth),
                static_cast<LONG>(m_DecoderParams.textureHeight)
            };

            // UPSCALER

            // Input
            m_InputArgsUpscaler[n].InputStream[0].pTexture2D = m_RGBTexture.Get();
            m_InputArgsUpscaler[n].InputStream[0].Subresource = 0;
            m_InputArgsUpscaler[n].InputStream[1].pTexture2D = nullptr;
            m_InputArgsUpscaler[n].InputStream[1].Subresource = 0;
            m_InputArgsUpscaler[n].Transform.Orientation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;
            m_InputArgsUpscaler[n].Transform.SourceRectangle = {
                0,
                0,
                static_cast<LONG>(m_DecoderParams.textureWidth),
                static_cast<LONG>(m_DecoderParams.textureHeight)
            };
            m_InputArgsUpscaler[n].Transform.DestinationRectangle = {
                0,
                0,
                static_cast<LONG>(m_OutputTextureInfo.width),
                static_cast<LONG>(m_OutputTextureInfo.height)
            };
            m_InputArgsUpscaler[n].Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
            m_InputArgsUpscaler[n].RateInfo.OutputIndex = 0;
            m_InputArgsUpscaler[n].RateInfo.InputFrameOrField = 0;
            m_InputArgsUpscaler[n].AlphaBlending.Enable = FALSE;
            m_InputArgsUpscaler[n].AlphaBlending.Alpha = 1.0f;
            m_InputArgsUpscaler[n].FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
            m_InputArgsUpscaler[n].FilterLevels[D3D12_VIDEO_PROCESS_FILTER_NOISE_REDUCTION] = m_NoiseReductionValue;
            m_InputArgsUpscaler[n].FilterLevels[D3D12_VIDEO_PROCESS_FILTER_EDGE_ENHANCEMENT] = m_EdgeEnhancementValue;

            // Output
            m_OutputArgsUpscaler[n].OutputStream[0].pTexture2D = m_OutputTexture.Get();
            m_OutputArgsUpscaler[n].OutputStream[0].Subresource = 0;
            m_OutputArgsUpscaler[n].OutputStream[1].pTexture2D = nullptr;
            m_OutputArgsUpscaler[n].OutputStream[1].Subresource = 0;
            m_OutputArgsUpscaler[n].TargetRectangle = {
                0,
                0,
                static_cast<LONG>(m_OutputTextureInfo.width),
                static_cast<LONG>(m_OutputTextureInfo.height)
            };

            // CONVERT & UPSCALER

            // Input
            m_InputArgsUpscalerConvert[n].InputStream[0].pTexture2D = m_FrameTexture.Get();
            m_InputArgsUpscalerConvert[n].InputStream[0].Subresource = 0;
            m_InputArgsUpscalerConvert[n].InputStream[1].pTexture2D = nullptr;
            m_InputArgsUpscalerConvert[n].InputStream[1].Subresource = 0;
            m_InputArgsUpscalerConvert[n].Transform.Orientation = D3D12_VIDEO_PROCESS_ORIENTATION_DEFAULT;
            m_InputArgsUpscalerConvert[n].Transform.SourceRectangle = {
                0,
                0,
                static_cast<LONG>(m_DecoderParams.textureWidth),
                static_cast<LONG>(m_DecoderParams.textureHeight)
            };
            m_InputArgsUpscalerConvert[n].Transform.DestinationRectangle = {
                0,
                0,
                static_cast<LONG>(m_OutputTextureInfo.width),
                static_cast<LONG>(m_OutputTextureInfo.height)
            };
            m_InputArgsUpscalerConvert[n].Flags = D3D12_VIDEO_PROCESS_INPUT_STREAM_FLAG_NONE;
            m_InputArgsUpscalerConvert[n].RateInfo.OutputIndex = 0;
            m_InputArgsUpscalerConvert[n].RateInfo.InputFrameOrField = 0;
            m_InputArgsUpscalerConvert[n].AlphaBlending.Enable = FALSE;
            m_InputArgsUpscalerConvert[n].AlphaBlending.Alpha = 1.0f;
            m_InputArgsUpscalerConvert[n].FieldType = D3D12_VIDEO_FIELD_TYPE_NONE;
            m_InputArgsUpscalerConvert[n].FilterLevels[D3D12_VIDEO_PROCESS_FILTER_NOISE_REDUCTION] = m_NoiseReductionValue;
            m_InputArgsUpscalerConvert[n].FilterLevels[D3D12_VIDEO_PROCESS_FILTER_EDGE_ENHANCEMENT] = m_EdgeEnhancementValue;

            // Output
            m_OutputArgsUpscalerConvert[n].OutputStream[0].pTexture2D = m_SkipRenderStep2 ? m_OutputTexture.Get() : m_RGBTextureUpscaled.Get();
            m_OutputArgsUpscalerConvert[n].OutputStream[0].Subresource = 0;
            m_OutputArgsUpscalerConvert[n].OutputStream[1].pTexture2D = nullptr;
            m_OutputArgsUpscalerConvert[n].OutputStream[1].Subresource = 0;
            m_OutputArgsUpscalerConvert[n].TargetRectangle = {
                0,
                0,
                static_cast<LONG>(m_OutputTextureInfo.width),
                static_cast<LONG>(m_OutputTextureInfo.height)
            };
        }
        
        m_VideoProcessorUpscalerConvertEnabled = m_VideoProcessorConvertEnabled && m_VideoProcessorUpscalerEnabled;

        // Convert YUV->RGB (RenderStep1)
        // Note: VideoProcessor1 does not support:
        // DXGI_FORMAT_Y410 input (780M)
        // DXGI_FORMAT_AYUV input (780M)
        // G2084 all (780M)
        if(m_VideoProcessorConvertEnabled){
            m_hr = m_VideoDevice->CreateVideoProcessor1(
                0,
                &outputStreamRGB,
                1,
                &inputStreamConvert,
                nullptr,
                IID_PPV_ARGS(&m_VideoProcessorConvert)
                );
            if(!verifyHResult(m_hr, "m_VideoDevice->CreateVideoProcessor1(... m_VideoProcessorConvert)")){
                return false;
            }
        }

        // Upscaler YUV (RenderStep1)
        // We exclude Upscaler RGB as YUV is a lot faster
        if(m_VideoProcessorUpscalerEnabled){
            m_hr = m_VideoDevice->CreateVideoProcessor1(
                0,
                &outputStreamRGB,
                1,
                &inputStreamUpscaler,
                nullptr,
                IID_PPV_ARGS(&m_VideoProcessorUpscaler)
                );
            if(!verifyHResult(m_hr, "m_VideoDevice->CreateVideoProcessor1(... m_VideoProcessorUpscaler)")){
                return false;
            }
        }

        // Upscaler + Convert YUV->RGB (RenderStep1)
        // RenderStep2 can be used for sharpening like RCAS (if m_EdgeEnhancementValue == 0)
        if(m_VideoProcessorUpscalerConvertEnabled){
            m_hr = m_VideoDevice->CreateVideoProcessor1(
                0,
                &outputStreamRGB,
                1,
                &inputStreamUpscalerConvert,
                nullptr,
                IID_PPV_ARGS(&m_VideoProcessorUpscalerConvert)
                );
            if(!verifyHResult(m_hr, "m_VideoDevice->CreateVideoProcessor1(... m_VideoProcessorUpscalerConvert)")){
                return false;
            }
        }
    }

    // If VideoProcessor is not available we fallback to Shader
    {
        if(!m_VideoProcessorConvertEnabled){
            
            // Render Step 1
            switch (m_RenderStep1) {
            case RenderStep::CONVERT_VIDEOPROCESSOR:
                m_RenderStep1 = RenderStep::CONVERT_SHADER;
                break;
            case RenderStep::ALL_VIDEOPROCESSOR:
                m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
                m_RenderStep1 = RenderStep::CONVERT_SHADER;
                m_RenderStep2 = RenderStep::UPSCALE_SHADER;
                m_InfoUpscaler = "FSR1 EASU";
                m_InfoSharpener = "FRS1 RCAS";
                m_InfoAlgo = "Shader FSR1";
                if(m_VendorHDRenabled){
                    m_InfoAlgo = m_InfoAlgo + " (SDR->HDR)";
                }
                m_VideoEnhancement->setAlgo(m_InfoAlgo);
                break;
            default:
                break;
            }
            
            // Render Step 2
            switch (m_RenderStep2) {
            case RenderStep::CONVERT_VIDEOPROCESSOR:
                m_RenderStep2 = RenderStep::CONVERT_SHADER;
                break;
            default:
                break;
            }
            
        }
        
        if(!m_VideoProcessorUpscalerEnabled){

            // Render Step 1
            switch (m_RenderStep1) {
            case RenderStep::ALL_VIDEOPROCESSOR:
                m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
                m_RenderStep1 = (m_IsTextureYUV444 && m_VideoProcessorConvertEnabled) ? RenderStep::CONVERT_VIDEOPROCESSOR : RenderStep::CONVERT_SHADER;
                m_RenderStep2 = RenderStep::UPSCALE_SHADER;
                m_InfoUpscaler = "FSR1 EASU";
                m_InfoSharpener = "FRS1 RCAS";
                m_InfoAlgo = "Shader FSR1";
                if(m_VendorHDRenabled){
                    m_InfoAlgo = m_InfoAlgo + " (SDR->HDR)";
                }
                m_VideoEnhancement->setAlgo(m_InfoAlgo);
                break;
            default:
                break;
            }

            // Render Step 2
            switch (m_RenderStep2) {
            case RenderStep::UPSCALE_VIDEOPROCESSOR:
                m_EnhancerType = D3D12VideoShaders::Enhancer::FSR1;
                m_RenderStep2 = RenderStep::UPSCALE_SHADER;
                m_InfoUpscaler = "FSR1 EASU";
                m_InfoAlgo = "Shader FSR1";
                if(m_VendorHDRenabled){
                    m_InfoAlgo = m_InfoAlgo + " (SDR->HDR)";
                }
                m_VideoEnhancement->setAlgo(m_InfoAlgo);
                break;
            default:
                break;
            }
        
        }
    }

    // Create frame resources (render target views)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = m_FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        m_hr = m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));
        if(!verifyHResult(m_hr, "m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RtvHeap));")){
            return false;
        }

        m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        m_BackBuffers.resize(m_FrameCount);
        m_BackBufferRTVs.resize(m_FrameCount);
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
        for (UINT n = 0; n < m_FrameCount; n++) {
            m_hr = m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_BackBuffers[n]));
            if(!verifyHResult(m_hr, "m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_BackBuffers[n]));")){
                return false;
            }
            m_Device->CreateRenderTargetView(m_BackBuffers[n].Get(), nullptr, rtvHandle);
            m_BackBufferRTVs[n] = rtvHandle;
            rtvHandle.Offset(1, m_RtvDescriptorSize);
        }
    }

    // Use the corresponding decoder (FFmpeg)
    {
        if (m_IsFrameD3D11) {
            // Use D3D11va to decode
            
            int err;

            // Initialize FFmpeg
            m_HwDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
            if (!m_HwDeviceContext) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to allocate D3D11VA device context");
                return false;
            }

            AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwDeviceContext->data;
            AVD3D11VADeviceContext* d3d11vaDeviceContext = (AVD3D11VADeviceContext*)deviceContext->hwctx;

            // FFmpeg will take ownership of these pointers, so we use CopyTo() to bump the ref count
            m_D3D11Device.CopyTo(&d3d11vaDeviceContext->device);
            m_D3D11DeviceContext.CopyTo(&d3d11vaDeviceContext->device_context);

            // Set lock functions that we will use to synchronize with FFmpeg's usage of our device context
            d3d11vaDeviceContext->lock = lockContext;
            d3d11vaDeviceContext->unlock = unlockContext;
            d3d11vaDeviceContext->lock_ctx = this;

            err = av_hwdevice_ctx_init(m_HwDeviceContext);
            if (err < 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to initialize D3D11VA device context: %d",
                             err);
                return false;
            }

        } else {
            // Use D3D12va to decode

            int err;

            m_HwDeviceContext = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D12VA);
            if (!m_HwDeviceContext) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to allocate D3D12VA device context");
                return false;
            }

            AVHWDeviceContext* deviceContext = (AVHWDeviceContext*)m_HwDeviceContext->data;
            AVD3D12VADeviceContext* d3d12vaDeviceContext = (AVD3D12VADeviceContext*)deviceContext->hwctx;

            // Attach Device
            d3d12vaDeviceContext->device = m_Device.Get();
            if (d3d12vaDeviceContext->device) {
                d3d12vaDeviceContext->device->AddRef();
            } else {
                av_buffer_unref(&m_HwDeviceContext);
                return false;
            }

            // Attach VideoDevice
            d3d12vaDeviceContext->video_device = m_VideoDevice.Get();
            if (d3d12vaDeviceContext->video_device) {
                d3d12vaDeviceContext->video_device->AddRef();
            } else {
                av_buffer_unref(&m_HwDeviceContext);
                return false;
            }

            // DirectX12 is Thread-safe by defaut as synchronisation is based on Fence,
            // therefor it is not necessary to lock/unlock in order to synchronise with ffmpeg
            d3d12vaDeviceContext->lock = [](void* ctx){}; // Do nothing
            d3d12vaDeviceContext->unlock = [](void* ctx){}; // Do nothing
            d3d12vaDeviceContext->lock_ctx = this;

            err = av_hwdevice_ctx_init(m_HwDeviceContext);
            if (err < 0) {
                d3d12vaDeviceContext->device->Release();
                av_buffer_unref(&m_HwDeviceContext);
                return false;
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "Failed to initialize D3D12VA device context: %d",
                             err);
                return false;
            }

        }
    }

    // Create fence for synchronization
    {
        m_hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceVideoProcess));
        if(!verifyHResult(m_hr, "m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceVideoProcess));")){
            return false;
        }
        m_FenceVideoProcessEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceVideoProcessEvent == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D12VideoDevice2::CreateEvent() failed: %x",
                         m_hr);
            return false;
        }

        m_hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceGraphics));
        if(!verifyHResult(m_hr, "m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceGraphics));")){
            return false;
        }
        m_FenceGraphicsEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceGraphicsEvent == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D12VideoDevice2::CreateEvent() failed: %x",
                         m_hr);
            return false;
        }

        m_hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceOverlay));
        if(!verifyHResult(m_hr, "m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceOverlay));")){
            return false;
        }
        m_FenceOverlayEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceOverlayEvent == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D12VideoDevice2::CreateEvent() failed: %x",
                         m_hr);
            return false;
        }

        m_hr = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceAMF));
        if(!verifyHResult(m_hr, "m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_FenceAMF));")){
            return false;
        }
        m_FenceAMFEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceAMFEvent == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "ID3D12VideoDevice2::CreateEvent() failed: %x",
                         m_hr);
            return false;
        }
    }

    // Prepare for the first frame
    {
        waitForVideoProcess(true);
        waitForGraphics(true);
        waitForOverlay(true);

        m_FenceVideoProcessValue = m_FenceVideoProcess->GetCompletedValue();
        m_FenceGraphicsValue = m_FenceGraphics->GetCompletedValue();
        m_FenceOverlayValue = m_FenceOverlay->GetCompletedValue();
        m_FenceAMFValue = m_FenceAMF->GetCompletedValue();
        
        m_VideoProcessCommandAllocator->Reset();
        m_VideoProcessCommandList->Reset(m_VideoProcessCommandAllocator.Get());
        m_GraphicsCommandAllocator->Reset();
        m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
        m_OverlayCommandAllocator->Reset();
        m_OverlayCommandList->Reset(m_OverlayCommandAllocator.Get(), nullptr);
    }

    // Initialize the Shaders
    {
        // Render Step 1
        switch (m_RenderStep1) {
        case RenderStep::CONVERT_SHADER:

            // Convert m_FrameTexture YUV (original size with alignment)
            // Pixel shader is faster 30% than compute shader
            m_ShaderConverter.reset();
            m_ShaderConverter = std::make_unique<D3D12VideoShaders>(
                m_Device.Get(),
                m_GraphicsCommandList.Get(),
                m_GraphicsCommandQueue.Get(),
                m_VideoEnhancement,
                m_FrameTexture.Get(),
                m_RGBTexture.Get(),
                m_DecoderParams.textureWidth,
                m_DecoderParams.textureHeight,
                0,
                0,
                D3D12VideoShaders::Enhancer::CONVERT_PS,
                m_Decoder.ColorSpace
                );
            if (!m_ShaderConverter)
                return false;

            break;
        default:
            break;
        }

        // Render Step 2
        switch (m_RenderStep2) {
        case RenderStep::CONVERT_SHADER:

            // Convert
            // Pixel shader is faster 30% than compute shader
            m_ShaderConverter.reset();
            m_ShaderConverter = std::make_unique<D3D12VideoShaders>(
                m_Device.Get(),
                m_GraphicsCommandList.Get(),
                m_GraphicsCommandQueue.Get(),
                m_VideoEnhancement,
                m_YUVTextureUpscaled.Get(),
                m_OutputTexture.Get(),
                m_OutputTextureInfo.width,
                m_OutputTextureInfo.height,
                0,
                0,
                D3D12VideoShaders::Enhancer::CONVERT_PS,
                m_Decoder.ColorSpace
                );
            if (!m_ShaderConverter)
                return false;

            break;
        case RenderStep::UPSCALE_SHADER:

            // Upscale RGB Only
            m_ShaderUpscaler.reset();
            m_ShaderUpscaler = std::make_unique<D3D12VideoShaders>(
                m_Device.Get(),
                m_GraphicsCommandList.Get(),
                m_GraphicsCommandQueue.Get(),
                m_VideoEnhancement,
                m_RGBTexture.Get(),
                m_OutputTexture.Get(),
                m_OutputTextureInfo.width,
                m_OutputTextureInfo.height,
                m_OutputTextureInfo.top,
                m_OutputTextureInfo.left,
                m_EnhancerType,
                m_Decoder.ColorSpace
                );
            if (!m_ShaderUpscaler)
                return false;

            break;
        case RenderStep::SHARPEN_SHADER:

            // Upscale RGB Only
            m_ShaderSharpener.reset();
            m_ShaderSharpener = std::make_unique<D3D12VideoShaders>(
                m_Device.Get(),
                m_GraphicsCommandList.Get(),
                m_GraphicsCommandQueue.Get(),
                m_VideoEnhancement,
                m_RGBTextureUpscaled.Get(),
                m_OutputTexture.Get(),
                m_OutputTextureInfo.width,
                m_OutputTextureInfo.height,
                m_OutputTextureInfo.top,
                m_OutputTextureInfo.left,
                m_EnhancerType,
                m_Decoder.ColorSpace
                );
            if (!m_ShaderSharpener)
                return false;

            break;
        default:
            break;
        }

        m_hr = m_GraphicsCommandList->Close();
        if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
            return false;
        }
        ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
        m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);

        waitForGraphics();

        m_GraphicsCommandAllocator->Reset();
        m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
    }

    // Initialize Upscaler
    {
        // Enable VSR feature
        if (m_VendorVSRenabled) {
            if (m_VideoEnhancement->isVendorAMD()) {
                enableAMDVideoSuperResolution();
            } else if (m_VideoEnhancement->isVendorIntel()) {
                enableIntelVideoSuperResolution();
            } else if (m_VideoEnhancement->isVendorNVIDIA()) {
                enableNvidiaVideoSuperResolution();
            }
        }

        // Enable SDR->HDR simulation feature if available.
        // Disable the feature when streaming in HDR
        if (m_VendorHDRenabled) {
            if (m_VideoEnhancement->isVendorAMD()) {
                enableAMDHDR();
            } else if (m_VideoEnhancement->isVendorIntel()) {
                enableIntelHDR();
            } else if (m_VideoEnhancement->isVendorNVIDIA()) {
                enableNvidiaHDR();
            }
        }
    }

    // Overlay
    {
        
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 16;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        
        m_hr = m_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_OverlaySrvHeap));
        if(!verifyHResult(m_hr, "m_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_OverlaySrvHeap));")){
            return false;
        }
        
        ComPtr<ID3DBlob> errorBlob;
        QByteArray hlslSource;

        // RootSignature
        CD3DX12_DESCRIPTOR_RANGE1 srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        CD3DX12_ROOT_PARAMETER1 rootParam;
        rootParam.InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

        CD3DX12_STATIC_SAMPLER_DESC staticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
        rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSigDesc.Desc_1_1.NumParameters = 1;
        rootSigDesc.Desc_1_1.pParameters = &rootParam;
        rootSigDesc.Desc_1_1.NumStaticSamplers = 1;
        rootSigDesc.Desc_1_1.pStaticSamplers = &staticSampler;
        rootSigDesc.Desc_1_1.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> serializedRootSig;
        D3D12SerializeVersionedRootSignature(
            &rootSigDesc, &serializedRootSig, &errorBlob);
        m_hr = m_Device->CreateRootSignature(
            0, serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&m_OverlayRootSignature));
        if(!verifyHResult(m_hr, "m_Device->CreateRootSignature(... m_OverlayRootSignature)")){
            return false;
        }
        // Compile Overlay Vertex shader
        QFile fileVS(":/enhancer/overlay_vs.hlsl");
        if (!fileVS.open(QIODevice::ReadOnly)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open overlay_vs.hlsl");
            return false;
        }
        hlslSource = fileVS.readAll();
        fileVS.close();
        ComPtr<ID3DBlob> shaderVSblob;
        m_hr = D3DCompile(hlslSource.constData(), hlslSource.size(), "overlay_vs.hlsl", nullptr, nullptr, "main", "vs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderVSblob, &errorBlob);
        if(!verifyHResult(m_hr, "D3DCompile(... overlay_vs)")){
            if (errorBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VS compile error: %s",
                             (char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        // Compile Overlay Pixel shader
        QFile filePS(":/enhancer/overlay_ps.hlsl");
        if (!filePS.open(QIODevice::ReadOnly)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open overlay_ps.hlsl");
            return false;
        }
        hlslSource = filePS.readAll();
        filePS.close();
        ComPtr<ID3DBlob> shaderPSblob;
        m_hr = D3DCompile(hlslSource.constData(), hlslSource.size(), "overlay_ps.hlsl", nullptr, nullptr, "main", "ps_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderPSblob, &errorBlob);
        if(!verifyHResult(m_hr, "D3DCompile(... overlay_ps.hlsl)")){
            if (errorBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VS compile error: %s",
                             (char*)errorBlob->GetBufferPointer());
            }
            return false;
        }

        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        auto& rt = blendDesc.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_ZERO;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_RASTERIZER_DESC rastDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        rastDesc.CullMode = D3D12_CULL_MODE_NONE;
        rastDesc.FillMode = D3D12_FILL_MODE_SOLID;

        D3D12_DEPTH_STENCIL_DESC depthDesc = {};
        depthDesc.DepthEnable = FALSE;
        depthDesc.StencilEnable = FALSE;

        // Pipeline
        DXGI_SWAP_CHAIN_DESC swapDesc;
        m_SwapChain->GetDesc(&swapDesc);
        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(VERTEX, x), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(VERTEX, u), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(VERTEX, r), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },            
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.pRootSignature = m_OverlayRootSignature.Get();
        psoDesc.VS = { shaderVSblob->GetBufferPointer(), shaderVSblob->GetBufferSize() };
        psoDesc.PS = { shaderPSblob->GetBufferPointer(), shaderPSblob->GetBufferSize() };
        psoDesc.RasterizerState = rastDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthDesc;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = swapDesc.BufferDesc.Format;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;
        psoDesc.NodeMask = 0;
        psoDesc.CachedPSO.pCachedBlob = nullptr;
        psoDesc.CachedPSO.CachedBlobSizeInBytes = 0;

        m_hr = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_OverlayPSO));
        if(!verifyHResult(m_hr, "m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_OverlayPSO));")){
            return false;
        }
    }

    m_TimerFPS.start();

    return true;
}

/**
 * \brief Wait for the frame to be processed
 *
 * Wait for the frame to be processed before rendering it.
 *
 * \param bool waitCPU, at true the CPU is blocked until the GPU is done
 * \return void
 */
void D3D12VARenderer::waitForVideoProcess(bool waitCPU)
{
    if(!m_FenceVideoProcess || !m_FenceVideoProcessEvent)
        return;
    
    // Insure to complete the process before presenting
    const UINT64 fence = ++m_FenceVideoProcessValue;
    m_hr = m_VideoProcessCommandQueue->Signal(m_FenceVideoProcess.Get(), fence);
    if(!verifyHResult(m_hr, "m_VideoProcessCommandQueue->Signal(m_FenceVideoProcess.Get(), fence);")){
        return;
    }
    if (!waitCPU) {
        if (m_Vsync) {
            m_GraphicsCommandQueue->Wait(m_FenceVideoProcess.Get(), fence);
            if(m_AmfCommandQueue) m_AmfCommandQueue->Wait(m_FenceVideoProcess.Get(), fence);
        }
    } else if (m_FenceVideoProcess->GetCompletedValue() < fence) {
        m_hr = m_FenceVideoProcess->SetEventOnCompletion(fence, m_FenceVideoProcessEvent);
        if(!verifyHResult(m_hr, "m_FenceVideoProcess->SetEventOnCompletion(fence, m_FenceVideoProcessEvent);")){
            return;
        }
        WaitForSingleObject(m_FenceVideoProcessEvent, INFINITE);
    }
}

/**
 * \brief Wait for the frame to be rendered
 *
 * Synchronizes CPU with GPU by waiting for the frame to finish rendering.
 * Updates the current back buffer index after synchronization.
 *
 * \param bool waitCPU, at true the CPU is blocked until the GPU is done
 * \return void
 */
void D3D12VARenderer::waitForGraphics(bool waitCPU)
{
    if(!m_FenceGraphics || !m_FenceGraphicsEvent)
        return;
    
    // Insure to complete the process before presenting
    const UINT64 fence = ++m_FenceGraphicsValue;
    m_hr = m_GraphicsCommandQueue->Signal(m_FenceGraphics.Get(), fence);
    if(!verifyHResult(m_hr, "m_GraphicsCommandQueue->Signal(m_FenceGraphics.Get(), fence);")){
        return;
    }
    if (!waitCPU) {
        if (m_Vsync) {
            m_VideoProcessCommandQueue->Wait(m_FenceGraphics.Get(), fence);
            if(m_AmfCommandQueue) m_AmfCommandQueue->Wait(m_FenceGraphics.Get(), fence);
        }
    } else if (m_FenceGraphics->GetCompletedValue() < fence) {
        m_hr = m_FenceGraphics->SetEventOnCompletion(fence, m_FenceGraphicsEvent);
        if(!verifyHResult(m_hr, "m_FenceGraphics->SetEventOnCompletion(fence, m_FenceGraphicsEvent);")){
            return;
        }
        WaitForSingleObject(m_FenceGraphicsEvent, INFINITE);
    }
}

/**
 * \brief Wait for the overlay to be ready
 *
 * Wait for the overlay to be ready before displaying it.
 *
 * \param bool waitCPU, at true the CPU is blocked until the GPU is done
 * \return void
 */
void D3D12VARenderer::waitForOverlay(bool waitCPU)
{
    if(!m_FenceOverlay || !m_FenceOverlayEvent)
        return;
    
    // Insure to complete the Overlay before presenting
    const UINT64 fence = ++m_FenceOverlayValue;
    m_hr = m_OverlayCommandQueue->Signal(m_FenceOverlay.Get(), fence);
    if(!verifyHResult(m_hr, "m_OverlayCommandQueue->Signal(m_FenceOverlay.Get(), fence);")){
        return;
    }
    if (!waitCPU) {
        m_VideoProcessCommandQueue->Wait(m_FenceOverlay.Get(), fence);
        m_GraphicsCommandQueue->Wait(m_FenceOverlay.Get(), fence);
        if(m_AmfCommandQueue) m_AmfCommandQueue->Wait(m_FenceOverlay.Get(), fence);
    } else if (m_FenceOverlay->GetCompletedValue() < fence) {
        m_hr = m_FenceOverlay->SetEventOnCompletion(fence, m_FenceOverlayEvent);
        if(!verifyHResult(m_hr, "m_FenceOverlay->SetEventOnCompletion(fence, m_FenceOverlayEvent);")){
            return;
        }
        WaitForSingleObject(m_FenceOverlayEvent, INFINITE);
    }
}

/**
 * \brief Update overlayed stats information
 *
 * Update the statistics information displayed as overlay
 * This function can be called on an arbitrary thread.
 *
 * \param Overlay::OverlayType type
 * \return void
 */
void D3D12VARenderer::notifyOverlayUpdated(Overlay::OverlayType type)
{
    if (m_OverlaySkip) {
        return;
    }
    
    m_OverlaySkip = true;
    
    SDL_Surface* newSurface = Session::get()->getOverlayManager().getUpdatedOverlaySurface(type);
    bool overlayEnabled = Session::get()->getOverlayManager().isOverlayEnabled(type);
    if (newSurface == nullptr) {
        m_OverlaySkip = false;
        return;
    }

    if (!overlayEnabled) {
        SDL_FreeSurface(newSurface);
        m_OverlaySkip = false;
        return;
    }
    
    SDL_AtomicLock(&m_OverlayLock);
    ComPtr<ID3D12Resource> oldTexture = std::move(m_OverlayTextures[type]);
    ComPtr<ID3D12Resource> oldVertexBuffer = std::move(m_OverlayVertexBuffers[type]);
    
    if (!m_NewTexture) {
        // Create texture resources
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = newSurface->w;
        texDesc.Height = newSurface->h;
        texDesc.MipLevels = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
        CD3DX12_HEAP_PROPERTIES heapDefaultProps(D3D12_HEAP_TYPE_DEFAULT);
    
        m_hr = m_Device->CreateCommittedResource(
            &heapDefaultProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_NewTexture));
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_NewTexture)")){
            SDL_AtomicUnlock(&m_OverlayLock);
            m_OverlaySkip = false;
            return;
        }
    
        // Create an upload heap to transfer texture data
        UINT64 uploadBufferSize;
        m_Device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
        CD3DX12_RESOURCE_DESC ubDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
        CD3DX12_HEAP_PROPERTIES heapUploadProps(D3D12_HEAP_TYPE_UPLOAD);
    
        m_hr = m_Device->CreateCommittedResource(
            &heapUploadProps,
            D3D12_HEAP_FLAG_NONE,
            &ubDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_TextureUploadHeap));
        if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... m_TextureUploadHeap)")){
            SDL_AtomicUnlock(&m_OverlayLock);
            m_OverlaySkip = false;
            return;
        }
        
        // Create SRV in a descriptor heap
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_OverlaySrvHeap->GetCPUDescriptorHandleForHeapStart());
        m_Device->CreateShaderResourceView(m_NewTexture.Get(), &srvDesc, m_OverlaySrvHeap->GetCPUDescriptorHandleForHeapStart());
        
        SDL_FRect renderRect = {};
        if (type == Overlay::OverlayStatusUpdate) {
            // Bottom Left
            renderRect.x = 0;
            renderRect.y = 0;
        }
        else if (type == Overlay::OverlayDebug) {
            // Top left
            renderRect.x = 0;
            renderRect.y = m_OutputTextureInfo.height - newSurface->h;
        }
        
        // Offsets
        renderRect.x += m_OutputTextureInfo.left;
        renderRect.y -= m_OutputTextureInfo.top;
    
        renderRect.w = newSurface->w;
        renderRect.h = newSurface->h;
    
        // Convert screen space to normalized device coordinates
        StreamUtils::screenSpaceToNormalizedDeviceCoords(&renderRect, m_OutputTextureInfo.width, m_OutputTextureInfo.height);
    
        // Create vertex buffer
        m_Verts[0] = {renderRect.x, renderRect.y, 0, 1, 0, 0, 0, 0.0f};
        m_Verts[1] = {renderRect.x, renderRect.y+renderRect.h, 0, 0, 0, 0, 0, 0.0f};
        m_Verts[2] = {renderRect.x+renderRect.w, renderRect.y, 1, 1, 0, 0, 0, 0.0f};
        m_Verts[3] = {renderRect.x+renderRect.w, renderRect.y+renderRect.h, 1, 0, 0, 0, 0, 0.0f};
    
        m_VbSize = sizeof(m_Verts);
    
        CD3DX12_HEAP_PROPERTIES uploadHeapVB(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(m_VbSize);
    
        m_Device->CreateCommittedResource(
            &uploadHeapVB,
            D3D12_HEAP_FLAG_NONE,
            &vbDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_NewVertexBuffer)
            );
    
        void* pMappedData = nullptr;
        m_NewVertexBuffer->Map(0, nullptr, &pMappedData);
        memcpy(pMappedData, m_Verts, m_VbSize);
        m_NewVertexBuffer->Unmap(0, nullptr);
    }
    
    // Copy data to the upload heap and then to the default texture
    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = newSurface->pixels;
    textureData.RowPitch = newSurface->pitch;
    textureData.SlicePitch = newSurface->pitch * newSurface->h;
    
    // The surface is no longer required
    SDL_FreeSurface(newSurface);
    newSurface = nullptr;
    
    m_OverlayCommandAllocator->Reset();
    m_OverlayCommandList->Reset(m_OverlayCommandAllocator.Get(), nullptr);
    
    UpdateSubresources(m_OverlayCommandList.Get(), m_NewTexture.Get(), m_TextureUploadHeap.Get(), 0, 0, 1, &textureData);
    
    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_NewTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
    m_OverlayCommandList->ResourceBarrier(1, &barrier);
    
    m_OverlayCommandList->Close();
    ID3D12CommandList* lists[] = { m_OverlayCommandList.Get() };
    m_OverlayCommandQueue->ExecuteCommandLists(1, lists);
    
    // Wait for the texture to be ready
    waitForOverlay(true);

    m_OverlayTextures[type] = std::move(m_NewTexture);
    m_OverlayVertexBuffers[type] = std::move(m_NewVertexBuffer);
    SDL_AtomicUnlock(&m_OverlayLock);
    
    m_OverlaySkip = false;
}

/**
 * \brief Render stats as overlay
 *
 * Render statitics information as overlay
 *
 * \param Overlay::OverlayType type
 * \return void
 */
void D3D12VARenderer::renderOverlay(Overlay::OverlayType type)
{
    if (!m_OverlaySrvHeap || !Session::get()->getOverlayManager().isOverlayEnabled(type)) {
        return;
    }
    
    ComPtr<ID3D12Resource> overlayTexture = m_OverlayTextures[type];
    ComPtr<ID3D12Resource> overlayVertexBuffer = m_OverlayVertexBuffers[type];

    if (!overlayTexture) {
        return;
    }
    
    // Record commands
    m_GraphicsCommandList->SetPipelineState(m_OverlayPSO.Get());
    m_GraphicsCommandList->SetGraphicsRootSignature(m_OverlayRootSignature.Get());
    
    ID3D12DescriptorHeap* heaps[] = { m_OverlaySrvHeap.Get() };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);
    
    D3D12_GPU_DESCRIPTOR_HANDLE srvGPU = m_OverlaySrvHeap->GetGPUDescriptorHandleForHeapStart();
    srvGPU.ptr += type * m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_GraphicsCommandList->SetGraphicsRootDescriptorTable(0, srvGPU);
    
    // Viewport
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = static_cast<float>(m_OutputTextureInfo.width);
    viewport.Height = static_cast<float>(m_OutputTextureInfo.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_GraphicsCommandList->RSSetViewports(1, &viewport);
    
    // Scissor
    D3D12_RECT scissor = {};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = m_OutputTextureInfo.width;
    scissor.bottom = m_OutputTextureInfo.height;
    m_GraphicsCommandList->RSSetScissorRects(1, &scissor);
    
    // Vertex buffer
    D3D12_VERTEX_BUFFER_VIEW m_VbView = {};
    m_VbView.BufferLocation = overlayVertexBuffer->GetGPUVirtualAddress();
    m_VbView.SizeInBytes = m_VbSize;
    m_VbView.StrideInBytes = sizeof(VERTEX);
    m_GraphicsCommandList->IASetVertexBuffers(0, 1, &m_VbView);
    m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    // Draw
    m_GraphicsCommandList->DrawInstanced(4, 1, 0, 0);
}

/**
 * \brief Render the frame
 *
 * Convert the frame YUV to RGB, and process it with an upscaler method if needed before render it.
 *
 * \param AVFrame* frame, the frame provided by the decoder FFmpeg
 * \return void
 */
void D3D12VARenderer::renderFrame(AVFrame* frame)
{
    bool resetVideoProcessCommand = false;
    bool resetGraphicsCommand = false;
    
    bool detachRGBTexture = false;
    bool detachRGBTextureUpscaled = false;
    bool detachYUVTextureUpscaled = false;
    bool detachOutputTexture = false;
    
    SDL_AtomicLock(&m_OverlayLock);

    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % 3;

    // Wait for previous frame to be rendered
    WaitForSingleObjectEx(m_FrameLatencyWaitableObject, 1000, true);

    m_Timer.start();
    TimerInfo("ms -----------------------------------------------", true);

    AVD3D12VAFrame* f = nullptr;
    UINT backBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
    CD3DX12_RESOURCE_BARRIER m_Barriers[2];
    
    // We simply skip the frame with the previous operation is not yet completed
    if (m_SkipFrame) {
        if (m_OutputTexturePrevious) {
            m_OutputTexture = m_OutputTexturePrevious;
            // Repeat previous Texture
            goto Draw;
        } else {
            // Draw a black screen
            goto Present;
        }
    }
    m_SkipFrame = true;

    if(m_IsFrameD3D11){
        // Some pipelines need D3D11
        
        if (m_RenderStep1 == RenderStep::UPSCALE_VPL) {
            // It does it own CopySubresourceRegion of the frame to be directly in the correct format
            goto RenderStep1;
        }
        
        lockContext(this);

        m_D3D11DeviceContext->CopySubresourceRegion(m_D3D11FrameTexture.Get(), 0, 0, 0, 0, (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1], &m_D3D11SrcBox);
        
        m_D3D11FenceValue++;
        
        m_D3D11DeviceContext->Signal(m_D3D11Fence.Get(), m_D3D11FenceValue);
        m_D3D11DeviceContext->Flush();
        
        unlockContext(this);
        
        m_VideoProcessCommandQueue->Wait(m_D3D12Fence.Get(), m_D3D11FenceValue);
        m_GraphicsCommandQueue->Wait(m_D3D12Fence.Get(), m_D3D11FenceValue);
        if(m_AmfCommandQueue) m_AmfCommandQueue->Wait(m_D3D12Fence.Get(), m_D3D11FenceValue);

    } else {
        // We default on D3D12
        
        f = reinterpret_cast<AVD3D12VAFrame*>(frame->data[0]);
        m_FrameTexture = f->texture;

        // In DX12, GPU work is asynchronous, so FFmpeg may still be writing to the texture.
        // We wait on the frame's fence to ensure the GPU has finished writing before use.
        // Regardless m_Vsync, we always force the synchronisation to avoid strong stuttering.
        if (f->sync_ctx.fence && f->sync_ctx.event) {
            m_VideoProcessCommandQueue->Wait(f->sync_ctx.fence, f->sync_ctx.fence_value);
            m_GraphicsCommandQueue->Wait(f->sync_ctx.fence, f->sync_ctx.fence_value);
            if(m_AmfCommandQueue) m_AmfCommandQueue->Wait(f->sync_ctx.fence, f->sync_ctx.fence_value);
        }

    }
    
RenderStep1:
    
    TimerInfo("ms (FFmpeg Frame)", true);

    // DebugExportToPNG(frameTexture, D3D12_RESOURCE_STATE_COMMON, "frameTexture.png");

    // VideoProcessor is used for the whole pipeline.
    // We convert directly frameTexture to m_OutputTexture.
    if(m_RenderStep1 == RenderStep::ALL_VIDEOPROCESSOR){

        resetVideoProcessCommand = true;

        m_InputArgsUpscalerConvert[m_CurrentFrameIndex].InputStream[0].pTexture2D = m_FrameTexture.Get();

        // Convert & Upscale
        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_FrameTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_SkipRenderStep2 ? m_OutputTexture.Get() : m_RGBTextureUpscaled.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_VideoProcessCommandList->ProcessFrames1(
            m_VideoProcessorUpscalerConvert.Get(),
            &m_OutputArgsUpscalerConvert[m_CurrentFrameIndex],
            1,
            &m_InputArgsUpscalerConvert[m_CurrentFrameIndex]
            );

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_FrameTexture.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_SkipRenderStep2 ? m_OutputTexture.Get() : m_RGBTextureUpscaled.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_hr = m_VideoProcessCommandList->Close();
        if(!verifyHResult(m_hr, "m_VideoProcessCommandList->Close();")){
            if (m_OutputTexturePrevious) {
                m_OutputTexture = m_OutputTexturePrevious;
                goto Draw;
            } else {
                goto Present;
            }
        }

        // Submit the command
        ID3D12CommandList* cmdLists[] = { m_VideoProcessCommandList.Get() };
        m_VideoProcessCommandQueue->ExecuteCommandLists(1, cmdLists);

        waitForVideoProcess();

        TimerInfo("ms (VP Upscale YUV + Convert YUV)", true);

        if(m_SkipRenderStep2){
            goto Draw;
        }

        goto RenderStep2;
    }

    // YUV->RGB Conversion using Video Procesor
    if(m_RenderStep1 == RenderStep::CONVERT_VIDEOPROCESSOR){

        resetVideoProcessCommand = true;

        m_InputArgsConvert[m_CurrentFrameIndex].InputStream[0].pTexture2D = m_FrameTexture.Get();

        // Convert
        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_FrameTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RGBTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_VideoProcessCommandList->ProcessFrames1(
            m_VideoProcessorConvert.Get(),
            &m_OutputArgsConvert[m_CurrentFrameIndex],
            1,
            &m_InputArgsConvert[m_CurrentFrameIndex]
            );

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_FrameTexture.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RGBTexture.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_hr = m_VideoProcessCommandList->Close();
        if(!verifyHResult(m_hr, "m_VideoProcessCommandList->Close();")){
            if (m_OutputTexturePrevious) {
                m_OutputTexture = m_OutputTexturePrevious;
                goto Draw;
            } else {
                goto Present;
            }
        }

        // Submit the command
        ID3D12CommandList* cmdLists[] = { m_VideoProcessCommandList.Get() };
        m_VideoProcessCommandQueue->ExecuteCommandLists(1, cmdLists);

        waitForVideoProcess();

        TimerInfo("ms (VP Convert YUV)", true);

        goto RenderStep2;
    }

    // YUV->RGB Conversion using Shader
    if(m_RenderStep1 == RenderStep::CONVERT_SHADER){

        resetGraphicsCommand = true;

        m_ShaderConverter->updateShaderResourceView(m_FrameTexture.Get());

        // Input: m_FrameTexture / Output: m_RGBTexture
        m_ShaderConverter->draw(
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON
            );

        if(m_RenderStep2 != RenderStep::UPSCALE_SHADER){
            m_hr = m_GraphicsCommandList->Close();
            if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
                if (m_OutputTexturePrevious) {
                    m_OutputTexture = m_OutputTexturePrevious;
                    goto Draw;
                } else {
                    goto Present;
                }
            }
            ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
            m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);

            waitForGraphics();
            m_GraphicsCommandAllocator->Reset();
            m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);

            resetGraphicsCommand = false;
        }

        TimerInfo("ms (Shader Convert YUV)", true);

        goto RenderStep2;
    }

    // AMF is used for the whole pipeline.
    // We convert directly m_FrameTexture to m_OutputTexture.
    if(m_RenderStep1 == RenderStep::ALL_AMF){

        m_AmfContext->CreateSurfaceFromDX12Native(m_FrameTexture.Get(), &m_AmfSurfaceYUV, nullptr);
        m_AmfSurfaceYUV->SetCrop(0, 0, m_DecoderParams.textureWidth, m_DecoderParams.textureHeight);
        
        // Upscaling
        m_AmfUpscalerYUV->SubmitInput(m_AmfSurfaceYUV);
        m_AmfUpscalerYUV->QueryOutput(&m_AmfData);

        // Convert to RGB
        m_AmfVideoConverterUpscaled->SubmitInput(m_AmfData);
        m_AmfVideoConverterUpscaled->QueryOutput(&m_AmfData);
        
        m_AmfCompute->FinishQueue();

        m_AmfData->QueryInterface(AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurfaceUpscaledRGB));
        ID3D12Resource* amfTexture = (ID3D12Resource*)m_AmfSurfaceUpscaledRGB->GetPlane(AMF_PLANE_PACKED)->GetNative();
        amfTexture->AddRef();
        
        if(m_SkipRenderStep2){
            m_OutputTexture.Attach(amfTexture);
            detachOutputTexture = true;
        } else {
            m_RGBTextureUpscaled.Attach(amfTexture);
            detachRGBTextureUpscaled = true;
        }

        TimerInfo("ms (AMF Upscale YUV -> AMF Convert RGB)", true);

        if(m_SkipRenderStep2){
            goto Draw;
        }

        goto RenderStep2;
    }

    // YUV Upscaling using AMF
    if(m_RenderStep1 == RenderStep::UPSCALE_AMF){

        m_AmfContext->CreateSurfaceFromDX12Native(m_FrameTexture.Get(), &m_AmfSurfaceYUV, nullptr);
        m_AmfSurfaceYUV->SetCrop(0, 0, m_DecoderParams.textureWidth, m_DecoderParams.textureHeight);

        m_AmfUpscalerYUV->SubmitInput(m_AmfSurfaceYUV);
        m_AmfUpscalerYUV->QueryOutput(&m_AmfData);
        m_AmfCompute->FinishQueue();

        m_AmfData->QueryInterface(AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurfaceUpscaledYUV));
        ID3D12Resource* amfTexture = (ID3D12Resource*)m_AmfSurfaceUpscaledYUV->GetPlane(AMF_PLANE_Y)->GetNative();
        amfTexture->AddRef();
        m_YUVTextureUpscaled.Attach(amfTexture);
        detachYUVTextureUpscaled = true;

        TimerInfo("ms (AMF Upscale YUV)", true);

        goto RenderStep2;
    }

    // YUV->RGB Conversion using AMF
    if(m_RenderStep1 == RenderStep::CONVERT_AMF){

        m_AmfContext->CreateSurfaceFromDX12Native(m_FrameTexture.Get(), &m_AmfSurfaceYUV, nullptr);
        m_AmfSurfaceYUV->SetCrop(0, 0, m_DecoderParams.textureWidth, m_DecoderParams.textureHeight);

        m_AmfVideoConverter->SubmitInput(m_AmfSurfaceYUV);
        m_AmfVideoConverter->QueryOutput(&m_AmfData);
        m_AmfCompute->FinishQueue();

        m_AmfData->QueryInterface(AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurfaceRGB));
        ID3D12Resource* amfTexture = (ID3D12Resource*)m_AmfSurfaceRGB->GetPlane(AMF_PLANE_PACKED)->GetNative();
        amfTexture->AddRef();
        m_RGBTexture.Attach(amfTexture);
        detachRGBTexture = true;

        TimerInfo("ms (AMF Convert YUV)", true);

        goto RenderStep2;
    }
    
    // YUV Upscaling using Intel VPL
    if(m_RenderStep1 == RenderStep::UPSCALE_VPL){
        
        // Working D3D11, but slow over x1.40
        mfxStatus sts;        
        VppSurface* vppSurfaceIn  = findUnlockedSurface(m_VppSurfaceIn);
        VppSurface* vppSurfaceOut = findUnlockedSurface(m_VppSurfaceOut);
        
        // IntelVPL upscaling
        bool vppError = false;
        
        // Ask IntelVPL to consume directly the texture given (zero-copy),
        // otherwise IntelVPL will copy to its internal allocated resource. 
        if (vppSurfaceIn || !vppSurfaceIn->surface) {
            sts = m_MemoryInterface->ImportFrameSurface(
                m_MemoryInterface,
                MFX_SURFACE_COMPONENT_VPP_INPUT,
                m_IsFrameD3D11 ? &vppSurfaceIn->textureLinkD3D11.SurfaceInterface.Header : &vppSurfaceIn->textureLinkD3D12.SurfaceInterface.Header,
                &vppSurfaceIn->surface);
                
            if (sts != MFX_ERR_NONE) {
                vppError = true;
            }
        } else {
            vppError = true;
        }
        
        if (vppError) {
            
            // Do nothing
            
        } else if (m_IsFrameD3D11) {
            
            lockContext(this);
            
            m_D3D11DeviceContext->CopySubresourceRegion(vppSurfaceIn->textureD3D11.Get(), 0, 0, 0, 0, (ID3D11Resource*)frame->data[0], (int)(intptr_t)frame->data[1], &m_D3D11SrcBox);
            
            m_D3D11FenceValue++;
            
            m_D3D11DeviceContext->Signal(m_D3D11Fence.Get(), m_D3D11FenceValue);
            m_D3D11DeviceContext->Flush();
            
            unlockContext(this);
            
            m_GraphicsCommandQueue->Wait(m_D3D12Fence.Get(), m_D3D11FenceValue);
            
        } else {
            // [Bug] Upscaling works (not faster as DX11), but NV12 copy operation has issue on the second plane UV.
            // It looks like the data is wrongly copied, resulting of UV at 0 and a green picture while converting into RGB.
            
            resetGraphicsCommand = true;
            
            // Copy
            m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_FrameTexture.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_SOURCE
                );
            m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                vppSurfaceIn->textureD3D12.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_COPY_DEST
                );
            m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);
            
            D3D12_RESOURCE_DESC inDesc = m_FrameTexture->GetDesc();
            UINT planeCount = 1;
            // Check 2-Planes
            switch (inDesc.Format) {
            case DXGI_FORMAT_NV12:
            case DXGI_FORMAT_P010:
                planeCount = 2;
                break;
            default:
                planeCount = 1;
                break;
            }
            
            for (UINT plane = 0; plane < planeCount; ++plane)
            {
                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = m_FrameTexture.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                src.SubresourceIndex = plane;
        
                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = vppSurfaceIn->textureD3D12.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = plane;
        
                D3D12_BOX srcBox = m_SrcBox;
                if (plane == 1) {
                    srcBox.bottom = inDesc.Height / 2;
                    srcBox.right = inDesc.Width;
                }
                
                m_GraphicsCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);
            }
    
            m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_FrameTexture.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_COMMON
                );
            m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                vppSurfaceIn->textureD3D12.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_COMMON
                );
            m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);
    
            m_hr = m_GraphicsCommandList->Close();
            ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
            m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);
            
            waitForGraphics(true);
            
        }
        
        // Process the upscaling
        sts = MFXVideoVPP_ProcessFrameAsync(m_VppSession, vppSurfaceIn->surface, &vppSurfaceOut->surface);
        
        if (sts == MFX_ERR_NONE && !vppError) {
            
            // Wait for the result
            sts = vppSurfaceOut->surface->FrameInterface->Synchronize(vppSurfaceOut->surface, 1000);
                
            if (sts == MFX_ERR_NONE) {
                
                // Copy the result into m_YUVTextureUpscaled
                
                if (m_IsFrameD3D11) {
                    
                    mfxHDL d3d11Tex = nullptr;
                    mfxResourceType resourceType = MFX_RESOURCE_DX11_TEXTURE;
                    vppSurfaceOut->surface->FrameInterface->GetNativeHandle(
                        vppSurfaceOut->surface,
                        &d3d11Tex,
                        &resourceType
                        );
                    
                    m_D3D11DeviceContext->CopySubresourceRegion(m_D3D11YUVTextureUpscaled.Get(), 0, 0, 0, 0, reinterpret_cast<ID3D11Texture2D*>(d3d11Tex), 0, &m_D3D11OutputBox);
                    
                    m_D3D11FenceValue++;
                    m_D3D11DeviceContext->Signal(m_D3D11Fence.Get(), m_D3D11FenceValue);
                    m_D3D11DeviceContext->Flush();
                    
                    m_GraphicsCommandQueue->Wait(m_D3D12Fence.Get(), m_D3D11FenceValue);
                    
                } else {
                    
                    mfxExtSurfaceD3D12Tex2DExportDescription d3d12ExportDesc = {};
                    d3d12ExportDesc.Header.BufferId = MFX_EXTBUFF_EXPORT_SHARING_DESC_D3D12;
                    d3d12ExportDesc.Header.BufferSz = sizeof(d3d12ExportDesc);
                    d3d12ExportDesc.d3d12Device     = m_Device.Get();
                    
                    mfxExtBuffer* extParams[1] = {};
                    extParams[0] = &d3d12ExportDesc.Header;
                    
                    mfxSurfaceHeader exportHeader = {};
                    exportHeader.SurfaceType  = MFX_SURFACE_TYPE_D3D12_TEX2D;
                    exportHeader.SurfaceFlags = MFX_SURFACE_FLAG_EXPORT_SHARED;
                    exportHeader.ExtParam     = extParams;
                    exportHeader.NumExtParam  = 1;
                    
                    mfxSurfaceHeader* exportedHeader = nullptr;
                    sts = vppSurfaceOut->surface->FrameInterface->Export(
                        vppSurfaceOut->surface,
                        exportHeader,
                        &exportedHeader
                    );
                    
                    auto* exportedD3D12 = reinterpret_cast<mfxSurfaceD3D12Tex2D*>(exportedHeader);
                    
                    ID3D12Resource* d3d12Tex = reinterpret_cast<ID3D12Resource*>(exportedD3D12->texture2D);
                    
                    DebugExportToPNG(d3d12Tex, D3D12_RESOURCE_STATE_COMMON, "d3d12Tex.png");
                    
                    m_GraphicsCommandAllocator->Reset();
                    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
                    
                    m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                        d3d12Tex,
                        D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_COPY_SOURCE
                        );
                    m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                        m_YUVTextureUpscaled.Get(),
                        D3D12_RESOURCE_STATE_COMMON,
                        D3D12_RESOURCE_STATE_COPY_DEST
                        );
                    m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);
            
                    D3D12_RESOURCE_DESC inDesc = d3d12Tex->GetDesc();
                    UINT planeCount = 1;
                    // Check 2-Planes
                    switch (inDesc.Format) {
                    case DXGI_FORMAT_NV12:
                    case DXGI_FORMAT_P010:
                        planeCount = 2;
                        break;
                    default:
                        planeCount = 1;
                        break;
                    }
                    
                    for (UINT plane = 0; plane < planeCount; ++plane)
                    {
                        D3D12_TEXTURE_COPY_LOCATION src = {};
                        src.pResource = d3d12Tex;
                        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                        src.SubresourceIndex = plane;
                
                        D3D12_TEXTURE_COPY_LOCATION dst = {};
                        dst.pResource = m_YUVTextureUpscaled.Get();
                        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                        dst.SubresourceIndex = plane;
                        
                        D3D12_BOX srcBox = m_OutputBox;
                        if (plane == 1) {
                            srcBox.bottom = inDesc.Height / 2;
                            srcBox.right = inDesc.Width;
                        }
                        
                        m_GraphicsCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &srcBox);
                    }
            
                    m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                        d3d12Tex,
                        D3D12_RESOURCE_STATE_COPY_SOURCE,
                        D3D12_RESOURCE_STATE_COMMON
                        );
                    m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                        m_YUVTextureUpscaled.Get(),
                        D3D12_RESOURCE_STATE_COPY_DEST,
                        D3D12_RESOURCE_STATE_COMMON
                        );
                    m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);
            
                    m_hr = m_GraphicsCommandList->Close();
                    if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
                        m_GraphicsCommandAllocator->Reset();
                        m_GraphicsCommandList->Reset(m_VideoProcessCommandAllocator.Get(), nullptr);
                        if (m_OutputTexturePrevious) {
                            m_OutputTexture = m_OutputTexturePrevious;
                            goto Draw;
                        } else {
                            goto Present;
                        }
                    }
                    
                    ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
                    m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);
                    
                    waitForGraphics(true);
                    
                    m_GraphicsCommandAllocator->Reset();
                    m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
                }
                
            } else if (sts == MFX_ERR_GPU_HANG || sts == MFX_WRN_IN_EXECUTION) {
                vppError = true;
            }
            
        } else {
            vppError = true;
        }
        
        if (vppError) {
            if (m_OutputTexturePrevious) {
                m_OutputTexture = m_OutputTexturePrevious;
                goto Draw;
            } else {
                goto Present;
            }
        }
        
        TimerInfo("ms (Intel VPL Convert YUV)", true);
        
        goto RenderStep2;
        
    }
    
RenderStep2:
    
    // D3D11DebugExportToPNG(m_D3D11FrameTexture.Get(), "m_D3D11FrameTexture.png");
    // DebugExportToPNG(m_FrameTexture.Get(), D3D12_RESOURCE_STATE_COMMON, "m_FrameTexture.png");
    // DebugExportToPNG(m_RGBTexture.Get(), D3D12_RESOURCE_STATE_COMMON, "m_RGBTexture.png");
    // DebugExportToPNG(m_YUVTextureUpscaled.Get(), D3D12_RESOURCE_STATE_COMMON, "m_YUVTextureUpscaled.png");
    
    // RGB Upscaling using VideoProcessor
    if(m_RenderStep2 == RenderStep::UPSCALE_VIDEOPROCESSOR){

        if(resetVideoProcessCommand){
            m_VideoProcessCommandAllocator->Reset();
            m_VideoProcessCommandList->Reset(m_VideoProcessCommandAllocator.Get());
        }
        resetVideoProcessCommand = true;

        m_InputArgsUpscaler[m_CurrentFrameIndex].InputStream[0].pTexture2D = m_RGBTexture.Get();

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RGBTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_OutputTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_VideoProcessCommandList->ProcessFrames1(
            m_VideoProcessorUpscaler.Get(),
            &m_OutputArgsUpscaler[m_CurrentFrameIndex],
            1,
            &m_InputArgsUpscaler[m_CurrentFrameIndex]
            );

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_RGBTexture.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_OutputTexture.Get(),
            D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_VideoProcessCommandList->ResourceBarrier(2, m_Barriers);

        m_hr = m_VideoProcessCommandList->Close();
        if(!verifyHResult(m_hr, "m_VideoProcessCommandList->Close();")){
            m_VideoProcessCommandAllocator->Reset();
            m_VideoProcessCommandList->Reset(m_VideoProcessCommandAllocator.Get());
            if (m_OutputTexturePrevious) {
                m_OutputTexture = m_OutputTexturePrevious;
                goto Draw;
            } else {
                goto Present;
            }
        }

        // Submit the command
        ID3D12CommandList* cmdLists[] = { m_VideoProcessCommandList.Get() };
        m_VideoProcessCommandQueue->ExecuteCommandLists(1, cmdLists);

        waitForVideoProcess();

        TimerInfo("ms (VP Upscale RGB)", true);

        goto Draw;
    }
    
    // RGB Upscaling using NVIDIA VSR
    if(m_RenderStep2 == RenderStep::UPSCALE_VSR){
        
        resetGraphicsCommand = true;
        
        NVSDK_NGX_VSR_QualityLevel vsrQuality = NVSDK_NGX_VSR_Quality_High;
        if(m_IsLowEndGPU) {
            vsrQuality = NVSDK_NGX_VSR_Quality_Medium;
        }
        if(m_IsOnBattery) {
            vsrQuality = NVSDK_NGX_VSR_Quality_Low;
        }
        
        // Setup VSR params
        NVSDK_NGX_D3D12_VSR_Eval_Params vsrEvalParams = {};
        vsrEvalParams.pInput                   = m_RGBTexture.Get();
        vsrEvalParams.pOutput                  = m_VendorHDRenabled ? m_RGBTextureUpscaled.Get() : m_OutputTexture.Get();
        vsrEvalParams.InputSubrectBase.X       = 0;
        vsrEvalParams.InputSubrectBase.Y       = 0;
        vsrEvalParams.InputSubrectSize.Width   = m_DecoderParams.textureWidth;
        vsrEvalParams.InputSubrectSize.Height  = m_DecoderParams.textureHeight;
        vsrEvalParams.OutputSubrectBase.X      = 0;
        vsrEvalParams.OutputSubrectBase.Y      = 0;
        vsrEvalParams.OutputSubrectSize.Width  = m_OutputTextureInfo.width;
        vsrEvalParams.OutputSubrectSize.Height = m_OutputTextureInfo.height;
        vsrEvalParams.QualityLevel             = vsrQuality;
        
        // Evaluate VSR
        NVSDK_NGX_Result ResultVSR = NGX_D3D12_EVALUATE_VSR_EXT(m_GraphicsCommandList.Get(), m_VSRFeature, m_VSRngxParameters, &vsrEvalParams);
        if (NVSDK_NGX_FAILED(ResultVSR)) {
            if (m_OutputTexturePrevious) {
                m_OutputTexture = m_OutputTexturePrevious;
                goto Draw;
            } else {
                goto Present;
            }
        }
        
        if(m_VendorHDRenabled){
            // Setup TrueHDR params
            NVSDK_NGX_D3D12_TRUEHDR_Eval_Params trueHDREvalParams = {};
            trueHDREvalParams.pInput                 = m_RGBTextureUpscaled.Get();
            trueHDREvalParams.pOutput                = m_OutputTexture.Get();
            trueHDREvalParams.InputSubrectTL.X       = 0;
            trueHDREvalParams.InputSubrectTL.Y       = 0;
            trueHDREvalParams.InputSubrectBR.Width   = m_OutputTextureInfo.width;
            trueHDREvalParams.InputSubrectBR.Height  = m_OutputTextureInfo.height;
            trueHDREvalParams.OutputSubrectTL.X      = 0;
            trueHDREvalParams.OutputSubrectTL.Y      = 0;
            trueHDREvalParams.OutputSubrectBR.Width  = m_OutputTextureInfo.width;
            trueHDREvalParams.OutputSubrectBR.Height = m_OutputTextureInfo.height;
            trueHDREvalParams.Contrast               = 100;
            trueHDREvalParams.Saturation             = 100;
            trueHDREvalParams.MiddleGray             = 50;
            trueHDREvalParams.MaxLuminance           = m_MaxLuminance;
            
            // Evaluate TrueHDR
            NVSDK_NGX_Result ResultTrueHDR = NGX_D3D12_EVALUATE_TRUEHDR_EXT(m_GraphicsCommandList.Get(), m_TrueHDRFeature, m_TrueHDRngxParameters, &trueHDREvalParams);
            if (NVSDK_NGX_FAILED(ResultTrueHDR)) {
                if (m_OutputTexturePrevious) {
                    m_OutputTexture = m_OutputTexturePrevious;
                    goto Draw;
                } else {
                    goto Present;
                }
            }
        }
        
        TimerInfo("ms (VSR Upscale RGB)", true);
        
        goto Draw;
    }
    
    // RGB Upscaling using AMD AMF
    if(m_RenderStep2 == RenderStep::UPSCALE_AMF){

        m_AmfContext->CreateSurfaceFromDX12Native(m_RGBTexture.Get(), &m_AmfSurfaceRGB, nullptr);

        m_AmfUpscalerRGB->SubmitInput(m_AmfSurfaceRGB);
        m_AmfUpscalerRGB->QueryOutput(&m_AmfData);
        m_AmfCompute->FinishQueue();

        m_AmfData->QueryInterface(AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurfaceUpscaledRGB));
        ID3D12Resource* amfTexture = (ID3D12Resource*)m_AmfSurfaceUpscaledRGB->GetPlane(AMF_PLANE_PACKED)->GetNative();
        amfTexture->AddRef();
        m_OutputTexture.Attach(amfTexture);
        detachOutputTexture = true;

        TimerInfo("ms (AMF Upscale RGB)", true);

        goto Draw;
    }

    // YUV->RGB Conversion using AMF
    if(m_RenderStep2 == RenderStep::CONVERT_AMF){

        m_AmfContext->CreateSurfaceFromDX12Native(m_YUVTextureUpscaled.Get(), &m_AmfSurfaceUpscaledYUV, nullptr);

        m_AmfVideoConverter->SubmitInput(m_AmfSurfaceUpscaledYUV);
        m_AmfVideoConverter->QueryOutput(&m_AmfData);
        m_AmfCompute->FinishQueue();

        m_AmfData->QueryInterface(AMFSurface::IID(), reinterpret_cast<void**>(&m_AmfSurfaceUpscaledRGB));
        ID3D12Resource* amfTexture = (ID3D12Resource*)m_AmfSurfaceUpscaledRGB->GetPlane(AMF_PLANE_PACKED)->GetNative();
        amfTexture->AddRef();
        m_OutputTexture.Attach(amfTexture);
        detachOutputTexture = true;

        TimerInfo("ms (AMF Convert YUV)", true);

        goto Draw;
    }

    // YUV->RGB Conversion using Shader
    if(m_RenderStep2 == RenderStep::CONVERT_SHADER){

        resetGraphicsCommand = true;

        m_ShaderConverter->updateShaderResourceView(m_YUVTextureUpscaled.Get());

        // Input: m_YUVTextureUpscaled / Output: m_OutputTexture
        m_ShaderConverter->draw(
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON
            );

        TimerInfo("ms (Shader Convert YUV)", true);

        goto Draw;
    }

    // RGB Upscaling using Shader
    if(m_RenderStep2 == RenderStep::UPSCALE_SHADER){

        resetGraphicsCommand = true;

        m_ShaderUpscaler->updateShaderResourceView(m_RGBTexture.Get());

        // Input: m_RGBTexture / Output: m_OutputTexture
        m_ShaderUpscaler->draw(
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON
            );

        TimerInfo("ms (Shader Upscale RGB)", true);

        goto Draw;
    }

    // RGB Sharpening using Shader
    if(m_RenderStep2 == RenderStep::SHARPEN_SHADER){

        resetGraphicsCommand = true;

        m_ShaderSharpener->updateShaderResourceView(m_RGBTextureUpscaled.Get());

        // Input: m_RGBTextureUpscaled / Output: m_OutputTexture
        m_ShaderSharpener->draw(
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COMMON
            );

        TimerInfo("ms (Shader Sharpen RGB)", true);

        goto Draw;
    }

Draw:
    
    // DebugExportToPNG(m_RGBTextureUpscaled.Get(), D3D12_RESOURCE_STATE_COMMON, "m_RGBTextureUpscaled.png");
    // DebugExportToPNG(m_OutputTexture.Get(), D3D12_RESOURCE_STATE_COMMON, "m_OutputTexture.png");

    // Copy the processed texture into the backbuffer to present
    {
        resetGraphicsCommand = true;

        // Add black background
        m_Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            m_BackBuffers[backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET
            );
        m_GraphicsCommandList->ResourceBarrier(1, &m_Barrier);

        m_GraphicsCommandList->OMSetRenderTargets(1, &m_BackBufferRTVs[backBufferIndex], FALSE, nullptr);
        
        FLOAT black[4] = {0, 0, 0, 1};
        m_GraphicsCommandList->ClearRenderTargetView(
            m_BackBufferRTVs[backBufferIndex],
            black,
            0,
            nullptr
            );

        // Copy
        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = m_OutputTexture.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = m_BackBuffers[backBufferIndex].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_OutputTexture.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_SOURCE
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_BackBuffers[backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST
            );
        m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);

        m_GraphicsCommandList->CopyTextureRegion(&dst, m_OutputTextureInfo.left, m_OutputTextureInfo.top, 0, &src, &m_OutputBox);

        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_OutputTexture.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_COMMON
            );
        m_Barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_BackBuffers[backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET
            );
        m_GraphicsCommandList->ResourceBarrier(2, m_Barriers);
        
        // Render overlays stats on top of the video stream
        // m_OverlaySkip is used to avoid race condition with overlay rendering
        bool overlaySkip = false;
        if (!m_OverlaySkip) {
            m_OverlaySkip = true;
            overlaySkip = true;
            for (int i = 0; i < Overlay::OverlayMax; i++) {
                renderOverlay((Overlay::OverlayType)i);
            }
        }
        
        m_Barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_BackBuffers[backBufferIndex].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT
            );
        m_GraphicsCommandList->ResourceBarrier(1, m_Barriers);

        m_hr = m_GraphicsCommandList->Close();
        if(!verifyHResult(m_hr, "m_GraphicsCommandList->Close();")){
            if (overlaySkip) {
                m_OverlaySkip = false;
            }
            goto Present;
        }
        ID3D12CommandList* cmdLists[] = { m_GraphicsCommandList.Get() };
        m_GraphicsCommandQueue->ExecuteCommandLists(1, cmdLists);
        
        if (overlaySkip) {
            m_OverlaySkip = false;
        }

        TimerInfo("ms (VP Copy m_OutputTexture -> m_BackBuffers)", true);
    }
    
Present:
    
    // Present the BackBuffer texture to the display
    {
        // When vSync is enabled, we do Present(0, 0) instead of (1, 0) to leverage VRR capability of the screen if any.        
        m_hr = m_SwapChain->Present(0, m_AllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
        
        TimerInfo("ms (Present)", true);

        // DebugExportToPNG(m_BackBuffers[backBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, "m_BackBuffers.png");

        if(!verifyHResult(m_hr, "m_SwapChain->Present(0, m_AllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0);")){
            // The card may have been removed or crashed. Reset the decoder.
            SDL_Event event;
            event.type = SDL_RENDER_TARGETS_RESET;
            SDL_AtomicUnlock(&m_OverlayLock);
            m_SkipFrame = false;
            SDL_PushEvent(&event);
            return;
        }
    }
    
    // Keep the pointer to the Output texture in case we need to skip the next frame.
    // It help to redraw the previous texture.
    m_OutputTexturePrevious = m_OutputTexture;
    
    // Detach textures pointer
    {
        if(detachRGBTexture){
            m_RGBTexture.Detach();
        }
        if(detachRGBTextureUpscaled){
            m_RGBTextureUpscaled.Detach();
        }
        if(detachYUVTextureUpscaled){
            m_YUVTextureUpscaled.Detach();
        }
        if(detachOutputTexture){
            m_OutputTexture.Detach();
        }
    }

    // Reset allocator and command list to record new commands
    {
        // VideoProcessor
        if(resetVideoProcessCommand){
            waitForVideoProcess(true); // Force the CPU waiting (true), otherwise the tearing is too agressive
            m_VideoProcessCommandAllocator->Reset();
            m_VideoProcessCommandList->Reset(m_VideoProcessCommandAllocator.Get());
        }

        // Graphics
        if(resetGraphicsCommand){
            waitForGraphics(true); // Force the CPU waiting (true), otherwise the tearing is too agressive
            m_GraphicsCommandAllocator->Reset();
            m_GraphicsCommandList->Reset(m_GraphicsCommandAllocator.Get(), nullptr);
        }

        TimerInfo("ms (Reinitialization)", true);
    }
    
    SDL_AtomicUnlock(&m_OverlayLock);
    m_SkipFrame = false;
    
    // Disable HDR renderer setting if the display HDR is not activate,
    // and reanable it if it does.
    updateDisplayHDRStatusAsync(frame->color_trc == AVCOL_TRC_SMPTE2084);

    return;
}


#ifndef QT_DEBUG

/**
 * \brief Export a texture D3D12 as PNG (Debug only)
 *
 * Export a texture as PNG at root directory of the application.
 * RGB and YUV in SDR are color correct.
 * HDR will display something wrong, but at least helpful to know it is loaded.
 * It generates a PNG only in Debug mode, just does nothing in Production.
 *
 * \param ID3D12Resource* srcTexture, Texture to export
 * \param D3D12_RESOURCE_STATES state, Current texture state
 * \param const char* filename, Filename used to export
 * \return void
 */
void D3D12VARenderer::DebugExportToPNG(
    ID3D12Resource* srcTexture,
    D3D12_RESOURCE_STATES state,
    const char* filename
    )
{
    return;
}

/**
 * \brief Export a texture D3D11 as PNG (Debug only)
 *
 * Export a texture as PNG at root directory of the application.
 * RGB and YUV in SDR are color correct.
 * HDR will display something wrong, but at least helpful to know it is loaded.
 * It generates a PNG only in Debug mode, just does nothing in Production.
 *
 * \param ID3D11Texture2D* srcTexture, Texture to export
 * \param const char* filename, Filename used to export
 * \return void
 */

void D3D12VARenderer::D3D11DebugExportToPNG(
    ID3D11Texture2D* srcTexture,
    const char* filename)
{
    return;
}

#else

// Used for debug purpose only
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/**
 * \brief Export a texture D3D12 as PNG (Debug only)
 *
 * Export a texture as PNG at root directory of the application.
 * RGB in SDR is color correct.
 * YUV in SDR displays only Y (grey).
 * HDR will display something wrong, but at least helpful to know it is loaded.
 * It generates a PNG only in Debug mode, just does nothing in Production.
 *
 * \param ID3D12Resource* srcTexture, Texture to export
 * \param D3D12_RESOURCE_STATES state, Current texture state
 * \param const char* filename, Filename used to export
 * \return void
 */
void D3D12VARenderer::DebugExportToPNG(
    ID3D12Resource* srcTexture,
    D3D12_RESOURCE_STATES state,
    const char* filename)
{    
    D3D12_RESOURCE_DESC desc = srcTexture->GetDesc();
    int textureWidth = desc.Width;
    int textureHeight = desc.Height;

    UINT64 totalBytes = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    m_Device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &totalBytes);

    ComPtr<ID3D12Resource> readback;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readback));

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = srcTexture;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = readback.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    m_Device->GetCopyableFootprints(&desc, 0, 1, 0, &dstLoc.PlacedFootprint, nullptr, nullptr, nullptr);

    m_PictureCommandAllocator->Reset();
    m_PictureCommandList->Reset(m_PictureCommandAllocator.Get(), nullptr);

    m_Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        srcTexture,
        state,
        D3D12_RESOURCE_STATE_COPY_SOURCE
        );
    m_PictureCommandList->ResourceBarrier(1, &m_Barrier);

    m_PictureCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    m_Barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        srcTexture,
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        state
        );
    m_PictureCommandList->ResourceBarrier(1, &m_Barrier);

    m_PictureCommandList->Close();
    ID3D12CommandList* lists[] = { m_PictureCommandList.Get() };
    m_PictureCommandQueue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    m_PictureCommandQueue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, evt);
    WaitForSingleObject(evt, INFINITE);
    CloseHandle(evt);

    void* mappedData;
    D3D12_RANGE readRange = { 0, totalBytes };
    readback->Map(0, &readRange, &mappedData);
    const UINT yPlaneSize = textureWidth * textureHeight;
    const uint8_t* sourceBytes = static_cast<const uint8_t*>(mappedData);

    const UINT rowPitch = layout.Footprint.RowPitch;

    if (desc.Format == DXGI_FORMAT_NV12) 
    {
        std::vector<uint8_t> yPlaneData(yPlaneSize);
        for (int y = 0; y < textureHeight; ++y) {
            memcpy(
                yPlaneData.data() + y * textureWidth,
                sourceBytes + y * rowPitch,
                textureWidth
                );
        }
        readback->Unmap(0, nullptr);
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            1,
            yPlaneData.data(),
            textureWidth
            );
        
        // std::vector<uint8_t> rgbData(textureWidth * textureHeight * 3);
        
        // const uint8_t* base = static_cast<const uint8_t*>(mappedData) + layout.Offset;
        // const uint8_t* yPlane  = base; // premier plan
        // const uint8_t* uvPlane = base + layout.Footprint.RowPitch * (desc.Height);
        
        // // const uint8_t* yPlane = sourceBytes;
        // // const uint8_t* uvPlane = sourceBytes + rowPitch * textureHeight;
        
        // for (int y = 0; y < textureHeight; ++y) {
        //     for (int x = 0; x < textureWidth; ++x) {
        //         // int yValue = yPlane[y * rowPitch + x];
        //         int yValue = yPlane[y * layout.Footprint.RowPitch + x];
                
        //         int uvRow = y / 2;
        //         int uvCol = x / 2;
        //         // int uvIndex = uvRow * rowPitch + uvCol * 2;
        //         int uvIndex = uvRow * layout.Footprint.RowPitch + uvCol * 2;
        //         int uValue = uvPlane[uvIndex] - 128;
        //         int vValue = uvPlane[uvIndex + 1] - 128;
                
        //         // Conversion YUV -> RGB (BT.709)
        //         int r = yValue + (int)(1.5748 * vValue);
        //         int g = yValue - (int)(0.1873 * uValue) - (int)(0.4681 * vValue);
        //         int b = yValue + (int)(1.8556 * uValue);
                
        //         // Clamp to [0, 255]
        //         r = (r < 0) ? 0 : (r > 255) ? 255 : r;
        //         g = (g < 0) ? 0 : (g > 255) ? 255 : g;
        //         b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                
        //         int pixelIndex = (y * textureWidth + x) * 3;
        //         rgbData[pixelIndex + 0] = (uint8_t)r;
        //         rgbData[pixelIndex + 1] = (uint8_t)g;
        //         rgbData[pixelIndex + 2] = (uint8_t)b;
        //     }
        // }
        
        // readback->Unmap(0, nullptr);
        
        // stbi_write_png(
        //     filename,
        //     textureWidth,
        //     textureHeight,
        //     3,
        //     rgbData.data(),
        //     textureWidth * 3
        // );
    }
    else if (desc.Format == DXGI_FORMAT_P010)
    {
        std::vector<uint8_t> rgbData(textureWidth * textureHeight * 3);
        
        const uint16_t* yPlane = reinterpret_cast<const uint16_t*>(sourceBytes);
        const uint16_t* uvPlane = reinterpret_cast<const uint16_t*>(sourceBytes + rowPitch * textureHeight);
        
        for (int y = 0; y < textureHeight; ++y) {
            for (int x = 0; x < textureWidth; ++x) {
                int yValue = yPlane[y * (rowPitch / 2) + x] >> 8;
                
                int uvRow = y / 2;
                int uvCol = x / 2;
                int uValue = (uvPlane[uvRow * (rowPitch / 2) + uvCol * 2] >> 8) - 128;
                int vValue = (uvPlane[uvRow * (rowPitch / 2) + uvCol * 2 + 1] >> 8) - 128;
                
                int r = yValue + (int)(1.5748 * vValue);
                int g = yValue - (int)(0.1873 * uValue) - (int)(0.4681 * vValue);
                int b = yValue + (int)(1.8556 * uValue);
                
                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                
                int pixelIndex = (y * textureWidth + x) * 3;
                rgbData[pixelIndex + 0] = (uint8_t)r;
                rgbData[pixelIndex + 1] = (uint8_t)g;
                rgbData[pixelIndex + 2] = (uint8_t)b;
            }
        }
        
        readback->Unmap(0, nullptr);
        
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            3,
            rgbData.data(),
            textureWidth * 3
        );
    } else if(desc.Format == DXGI_FORMAT_AYUV || desc.Format == DXGI_FORMAT_Y410) {
        std::vector<uint8_t> yPlaneData(yPlaneSize);
        for (int y = 0; y < textureHeight; ++y) {
            memcpy(
                yPlaneData.data() + y * textureWidth,
                sourceBytes + y * rowPitch,
                textureWidth
                );
        }
        readback->Unmap(0, nullptr);
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            1,
            yPlaneData.data(),
            textureWidth
            );
    } else {
        // RGB/RGBA formats
        std::vector<uint8_t> rgbData(textureWidth * textureHeight * 4);
        for (int y = 0; y < textureHeight; ++y) {
            memcpy(
                rgbData.data() + y * textureWidth * 4,
                sourceBytes + y * rowPitch,
                textureWidth * 4
                );
        }
        readback->Unmap(0, nullptr);
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            4,
            rgbData.data(),
            textureWidth * 4
            );
    }
}

/**
 * \brief Export a texture D3D11 as PNG (Debug only)
 *
 * Export a texture as PNG at root directory of the application.
 * RGB and YUV in SDR are color correct.
 * HDR will display something wrong, but at least helpful to know it is loaded.
 * It generates a PNG only in Debug mode, just does nothing in Production.
 *
 * \param ID3D11Texture2D* srcTexture, Texture to export
 * \param const char* filename, Filename used to export
 * \return void
 */
void D3D12VARenderer::D3D11DebugExportToPNG(
    ID3D11Texture2D* srcTexture,
    const char* filename)
{
    if (!srcTexture || !m_D3D11Device || !m_D3D11DeviceContext)
        return;
    
    D3D11_TEXTURE2D_DESC desc;
    srcTexture->GetDesc(&desc);
    
    int textureWidth = desc.Width;
    int textureHeight = desc.Height;
    
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;
    
    ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = m_D3D11Device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr))
        return;
    
    m_D3D11DeviceContext->CopyResource(stagingTexture.Get(), srcTexture);
    
    const UINT yPlaneSize = textureWidth * textureHeight;
    
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = m_D3D11DeviceContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr))
        return;
    
    const uint8_t* sourceBytes = static_cast<const uint8_t*>(mappedResource.pData);
    const UINT rowPitch = mappedResource.RowPitch;
    
    if (desc.Format == DXGI_FORMAT_NV12) {
        std::vector<uint8_t> rgbData(textureWidth * textureHeight * 3);
        
        const uint8_t* yPlane = sourceBytes;
        const uint8_t* uvPlane = sourceBytes + rowPitch * textureHeight;
        
        for (int y = 0; y < textureHeight; ++y) {
            for (int x = 0; x < textureWidth; ++x) {
                int yValue = yPlane[y * rowPitch + x];
                
                int uvRow = y / 2;
                int uvCol = x / 2;
                int uvIndex = uvRow * rowPitch + uvCol * 2;
                int uValue = uvPlane[uvIndex] - 128;
                int vValue = uvPlane[uvIndex + 1] - 128;
                
                // Conversion YUV -> RGB (BT.709)
                int r = yValue + (int)(1.5748 * vValue);
                int g = yValue - (int)(0.1873 * uValue) - (int)(0.4681 * vValue);
                int b = yValue + (int)(1.8556 * uValue);
                
                // Clamp to [0, 255]
                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                
                int pixelIndex = (y * textureWidth + x) * 3;
                rgbData[pixelIndex + 0] = (uint8_t)r;
                rgbData[pixelIndex + 1] = (uint8_t)g;
                rgbData[pixelIndex + 2] = (uint8_t)b;
            }
        }
        
        m_D3D11DeviceContext->Unmap(stagingTexture.Get(), 0);
        
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            3,
            rgbData.data(),
            textureWidth * 3
        );
    } else if (desc.Format == DXGI_FORMAT_P010) {
        std::vector<uint8_t> rgbData(textureWidth * textureHeight * 3);
        
        const uint16_t* yPlane = reinterpret_cast<const uint16_t*>(sourceBytes);
        const uint16_t* uvPlane = reinterpret_cast<const uint16_t*>(sourceBytes + rowPitch * textureHeight);
        
        for (int y = 0; y < textureHeight; ++y) {
            for (int x = 0; x < textureWidth; ++x) {
                int yValue = yPlane[y * (rowPitch / 2) + x] >> 8;
                
                int uvRow = y / 2;
                int uvCol = x / 2;
                int uValue = (uvPlane[uvRow * (rowPitch / 2) + uvCol * 2] >> 8) - 128;
                int vValue = (uvPlane[uvRow * (rowPitch / 2) + uvCol * 2 + 1] >> 8) - 128;
                
                int r = yValue + (int)(1.5748 * vValue);
                int g = yValue - (int)(0.1873 * uValue) - (int)(0.4681 * vValue);
                int b = yValue + (int)(1.8556 * uValue);
                
                r = (r < 0) ? 0 : (r > 255) ? 255 : r;
                g = (g < 0) ? 0 : (g > 255) ? 255 : g;
                b = (b < 0) ? 0 : (b > 255) ? 255 : b;
                
                int pixelIndex = (y * textureWidth + x) * 3;
                rgbData[pixelIndex + 0] = (uint8_t)r;
                rgbData[pixelIndex + 1] = (uint8_t)g;
                rgbData[pixelIndex + 2] = (uint8_t)b;
            }
        }
        
        m_D3D11DeviceContext->Unmap(stagingTexture.Get(), 0);
        
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            3,
            rgbData.data(),
            textureWidth * 3
        );
    } else if(desc.Format == DXGI_FORMAT_AYUV || desc.Format == DXGI_FORMAT_Y410) {
        std::vector<uint8_t> yPlaneData(yPlaneSize);
        for (int y = 0; y < textureHeight; ++y) {
            memcpy(
                yPlaneData.data() + y * textureWidth,
                sourceBytes + y * rowPitch,
                textureWidth
                );
        }
        
        m_D3D11DeviceContext->Unmap(stagingTexture.Get(), 0);
        
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            1,
            yPlaneData.data(),
            textureWidth
            );
    } else {
        // RGB/RGBA formats
        std::vector<uint8_t> rgbData(textureWidth * textureHeight * 4);
        for (int y = 0; y < textureHeight; ++y) {
            memcpy(
                rgbData.data() + y * textureWidth * 4,
                sourceBytes + y * rowPitch,
                textureWidth * 4
            );
        }
        
        m_D3D11DeviceContext->Unmap(stagingTexture.Get(), 0);
        
        stbi_write_png(
            filename,
            textureWidth,
            textureHeight,
            4,
            rgbData.data(),
            textureWidth * 4
        );
    }
}

#endif
