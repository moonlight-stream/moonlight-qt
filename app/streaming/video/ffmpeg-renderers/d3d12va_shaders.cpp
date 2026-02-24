#include <directx/d3dx12.h>
#include "d3d12va_shaders.h"
#include "SDL_log.h"
#include "qdebug.h"
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <cmath>
#include <QFile>
#include <QByteArray>

using Microsoft::WRL::ComPtr;

/**
 * \brief Custom IDxcIncludeHandler for Qt resources
 *
 * This class implements IDxcIncludeHandler to allow the DirectX Shader Compiler
 * (DXC) to load HLSL include files directly from Qt resources (e.g., ":/enhancer/").
 * When a shader contains #include "filename.hlsl", this handler provides
 * the corresponding resource to the compiler.
 */
class QtIncludeHandler : public IDxcIncludeHandler {
public:
    QtIncludeHandler() : m_refCount(1) {}
    virtual ~QtIncludeHandler() = default;
    
    HRESULT STDMETHODCALLTYPE LoadSource(
        LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override
    {
        QString path = QString::fromWCharArray(pFilename);
        QFile file(":/enhancer/" + path);
        if (!file.open(QIODevice::ReadOnly))
            return E_FAIL;
        
        QByteArray data = file.readAll();
        file.close();
        
        IDxcBlobEncoding* blob = nullptr;
        ComPtr<IDxcLibrary> library;
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
        library->CreateBlobWithEncodingOnHeapCopy(
            data.constData(), data.size(), CP_UTF8, &blob);
        
        *ppIncludeSource = blob;
        return S_OK;
    }
    
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == __uuidof(IDxcIncludeHandler)) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refCount; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = --m_refCount;
        if (count == 0) delete this;
        return count;
    }
    
private:
    std::atomic<ULONG> m_refCount;
};

/**
 * \brief Constructor
 */
D3D12VideoShaders::D3D12VideoShaders(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* graphicsCommandList,
    ID3D12CommandQueue* graphicsCommandQueue,
    VideoEnhancement* videoEnhancement,
    ID3D12Resource* textureIn,
    ID3D12Resource* textureOut,
    int outWidth,
    int outHeight,
    int offsetTop,
    int offsetLeft,
    Enhancer enhancer,
    DXGI_COLOR_SPACE_TYPE colorSpace
    ) :
    m_Device(device),
    m_GraphicsCommandList(graphicsCommandList),
    m_GraphicsCommandQueue(graphicsCommandQueue),
    m_VideoEnhancement(videoEnhancement),
    m_TextureIn(textureIn),
    m_TextureOut(textureOut),
    m_OutWidth(outWidth),
    m_OutHeight(outHeight),
    m_OffsetTop(offsetTop),
    m_OffsetLeft(offsetLeft),
    m_Enhancer(enhancer),
    m_ColorSpace(colorSpace),
    m_isYUV(false),
    m_IsYUV444(false),
    m_IsHDR(false),
    m_Is2Planes(false)
{
    // Texture must be provided
    if (!m_TextureIn || !m_TextureOut) {
        return;
    }
    
    D3D12_RESOURCE_DESC inDesc = m_TextureIn->GetDesc();
    m_InWidth = static_cast<int>(inDesc.Width);
    m_InHeight = static_cast<int>(inDesc.Height);
    
    // YUV
    switch (inDesc.Format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
        m_isYUV = true;
        break;
    default:
        m_isYUV = false;
        break;
    }
    
    // YUV 4:4:4
    switch (inDesc.Format) {
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
        m_IsYUV444 = true;
        break;
    default:
        m_IsYUV444 = false;
        break;
    }
    
    // HDR
    switch (inDesc.Format) {
    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
        m_IsHDR = true;
        break;
    default:
        m_IsHDR = false;
        break;
    }
    
    // 2-Planes
    switch (inDesc.Format) {
    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_P010:
        m_Is2Planes = true;
        break;
    default:
        m_Is2Planes = false;
        break;
    }
    
    // Viewport (Must match the Input texture, otherwise it will stretch the picture)
    m_Viewport.TopLeftX = -static_cast<float>(m_OffsetLeft);
    m_Viewport.TopLeftY = -static_cast<float>(m_OffsetTop);
    m_Viewport.Width = static_cast<float>(m_InWidth);
    m_Viewport.Height = static_cast<float>(m_InHeight);
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;
    
    // Scissor (Cropt to fit into the Output texture)
    m_ScissorRect.left = 0;
    m_ScissorRect.top = 0;
    m_ScissorRect.right = m_OutWidth;
    m_ScissorRect.bottom = m_OutHeight;
    
    m_IsUpscaling = isUpscaler(m_Enhancer);
    m_IsUsingShader = isUsingShader(m_Enhancer);
    
    m_AdvancedShader = isSupportingAdvancedShader();
    
    switch (m_Enhancer) {
    case Enhancer::CONVERT_PS:
        initializeCONVERT_PS();
        break;
    case Enhancer::NIS:
        initializeNIS();
        break;
    case Enhancer::NIS_SHARPENER:
        initializeNIS(false);
        break;
    case Enhancer::FSR1:
        initializeFSR1();
        break;
    case Enhancer::RCAS:
        initializeRCAS();
        break;
    case Enhancer::COPY:
        initializeCOPY();
        break;
    default:
        break;
    }
}

/**
 * \brief Destructor
 */
D3D12VideoShaders::~D3D12VideoShaders()
{
    m_TextureInterm.Reset();
    m_TextureIn.Reset();
    m_TextureOut.Reset();
    m_PipelineState.Reset();
    m_RootSignature.Reset();
    m_DescriptorHeapCBV_SRV_UAV.Reset();
    m_DescriptorHeapRTV.Reset();
    m_DescriptorHeapSampler.Reset();
    
    m_GraphicsCommandList.Reset();
    m_GraphicsCommandQueue.Reset();
    
    m_Device.Reset();
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
bool D3D12VideoShaders::verifyHResult(HRESULT hr, const char* operation)
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
 * \brief Check if enhancer supports advanced shaders
 *
 * Tests for 16-bit precision support and attempts to compile
 * an advanced shader to determine hardware compatibility.
 *
 * \return bool True if advanced shaders are supported
 */
bool D3D12VideoShaders::isSupportingAdvancedShader()
{
    // Check 16-bit support
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    m_hr = m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options));
    if(!verifyHResult(m_hr, "m_Device->CheckFeatureSupport(...options)")){
        return false;
    }
    if (!(options.MinPrecisionSupport & D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT)){
        return false;
    }
    
    // Try to load an advanced shader
    m_AdvancedShader = true;
    bool convertPS = initializeCONVERT_PS();
    
    // Reset variables
    m_PipelineState.Reset();
    m_RootSignature.Reset();
    m_DescriptorHeapCBV_SRV_UAV.Reset();
    m_DescriptorHeapRTV.Reset();
    m_DescriptorHeapSampler.Reset();
    
    if(!convertPS){
        return false;
    }
    
    return true;
}

/**
 * \brief Check if enhancer is an upscaler
 *
 * Determines if the specified enhancer performs upscaling.
 *
 * \param Enhancer enhancer Enhancer type to check
 * \return bool True if enhancer is an upscaler
 */
bool D3D12VideoShaders::isUpscaler(Enhancer enhancer)
{
    switch (enhancer) {
    case Enhancer::FSR1:
    case Enhancer::NIS:
        return true;
    default:
        return false;
    }
}

/**
 * \brief Check if enhancer is a sharpener
 *
 * Determines if the specified enhancer performs sharpening.
 *
 * \param Enhancer enhancer Enhancer type to check
 * \return bool True if enhancer is a sharpener
 */
bool D3D12VideoShaders::isSharpener(Enhancer enhancer)
{
    switch (enhancer) {
    case Enhancer::FSR1:
    case Enhancer::NIS:
    case Enhancer::RCAS:
    case Enhancer::NIS_SHARPENER:
        return true;
    default:
        return false;
    }
}

/**
 * \brief Check if enhancer uses shaders
 *
 * Determines if the enhancer requires shader processing.
 *
 * \param Enhancer enhancer Enhancer type to check
 * \return bool True if enhancer uses shaders
 */
bool D3D12VideoShaders::isUsingShader(Enhancer enhancer)
{
    return enhancer != Enhancer::NONE;
}

/**
 * \brief Update shader resource views for input texture
 *
 * Creates SRVs based on texture format (RGB, NV12/P010, AYUV/Y410)
 * with appropriate DXGI formats for each plane.
 *
 * \param ID3D12Resource* resource Input texture resource
 * \return bool True if SRVs created successfully
 */
bool D3D12VideoShaders::updateShaderResourceView(ID3D12Resource* resource)
{
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    
    // RGB
    if (!m_isYUV) {
        // t0
        if (!createSRVforResource(resource, 0, desc.Format, 0)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createSRVforResource(input) failed");
            return false;
        }
    }

    // NV12 or P010
    else if (m_Is2Planes) {
        // t0: Prepare Luma plane
        DXGI_FORMAT formatY = m_IsHDR ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
        if (!createSRVforResource(resource, 0, formatY, 0)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createSRVforResource(input) failed");
            return false;
        }

        // t1: Prepare Chroma plane
        DXGI_FORMAT formatUV = m_IsHDR ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
        if (!createSRVforResource(resource, 1, formatUV, 1)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createSRVforResource(input) failed");
            return false;
        }
    }

    // AYUV or Y410
    else {
        // t0: AYUV and Y410 only use 1 plan
        if (!createSRVforResource(resource, 0, desc.Format, 0)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createSRVforResource(input) failed");
            return false;
        }
    }

    return true;
}

/**
 * \brief Update YUV offset constants
 *
 * Sets the YUV channel offset values in root constants.
 *
 * \param XMFLOAT3 offset Offset values for Y, U, V channels
 */
void D3D12VideoShaders::updateRootConstsOffset(XMFLOAT3 offset){
    m_RootConsts.g_OffsetY = offset.x;
    m_RootConsts.g_OffsetU = offset.y;
    m_RootConsts.g_OffsetV = offset.z;
}

/**
 * \brief Initialize root constants structure
 *
 * Sets up conversion matrices, gamma correction, color range,
 * and format-specific constants based on input texture properties.
 */
void D3D12VideoShaders::initializeRootConsts()
{
    m_RootConsts = {};

    // SDR and HDR invert bits
    m_RootConsts.g_INV_8BIT  = 1.0f / 255.0f;
    m_RootConsts.g_INV_10BIT = 1.0f / 1023.0f;

    // PQ constants
    m_RootConsts.g_M1Inv = 16384.0f / 2610.0f;
    m_RootConsts.g_M2Inv = 4096.0f / (2523.0f * 128.0f);//32.0f / 2523.0f;
    m_RootConsts.g_C1    = 3424.0f / 4096.0f;
    m_RootConsts.g_C2    = 2413.0f / 128.0f;
    m_RootConsts.g_C3    = 2392.0f / 128.0f;

    // Texture format In/Out
    D3D12_RESOURCE_DESC desc = m_TextureIn->GetDesc();
    DXGI_FORMAT formatIn = desc.Format;
    switch (formatIn) {
    case DXGI_FORMAT_NV12:
        m_RootConsts.g_InputFormat = 0; // FMT_NV12
        m_RootConsts.g_OutputFormat = 0; // OUT_RGBA8;
        break;
    case DXGI_FORMAT_P010:
        m_RootConsts.g_InputFormat = 1; // FMT_P010
        m_RootConsts.g_OutputFormat = 1; // OUT_RGB10;
        break;
    case DXGI_FORMAT_AYUV:
        m_RootConsts.g_InputFormat = 2; // FMT_AYUV
        m_RootConsts.g_OutputFormat = 0; // OUT_RGBA8;
        break;
    case DXGI_FORMAT_Y410:
        m_RootConsts.g_InputFormat = 3; // FMT_Y410
        m_RootConsts.g_OutputFormat = 1; // OUT_RGB10;
        break;
    default:
        m_RootConsts.g_InputFormat = 0; // FMT_NV12
        m_RootConsts.g_OutputFormat = 0; // OUT_RGBA8;
        break;
    }

    // Gamma correction (not needed)
    switch (m_ColorSpace) {
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
        m_RootConsts.g_GammaCorrection = 1; // GC_G22
        break;
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
        m_RootConsts.g_GammaCorrection = 2; // GC_G24
        break;
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
        m_RootConsts.g_GammaCorrection = 3; // GC_PQ
        break;
    default:
        m_RootConsts.g_GammaCorrection = 0; // GC_LINEAR
        break;
    }

    switch (m_ColorSpace) {
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
        m_RootConsts.g_Range = 1; // CR_FULL
        break;
    default:
        m_RootConsts.g_Range = 0; // CR_LIMTED
        break;
    }

    // Convertion matrix
    XMFLOAT3 g_CSC_Row0;
    XMFLOAT3 g_CSC_Row1;
    XMFLOAT3 g_CSC_Row2;
    switch (m_ColorSpace) {
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601:
        // BT.601 limited
        g_CSC_Row0 = { 1.1644f,  0.0000f,  1.5960f};
        g_CSC_Row1 = { 1.1644f, -0.3918f, -0.8130f};
        g_CSC_Row2 = { 1.1644f,  2.0172f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601:
        // BT.601 full
        g_CSC_Row0 = { 1.0000f,  0.0000f,  1.4020f};
        g_CSC_Row1 = { 1.0000f, -0.3441f, -0.7141f};
        g_CSC_Row2 = { 1.0000f,  1.7720f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709:
        // BT.709 limited
        g_CSC_Row0 = { 1.1644f,  0.0000f,  1.7927f};
        g_CSC_Row1 = { 1.1644f, -0.2132f, -0.5329f};
        g_CSC_Row2 = { 1.1644f,  2.1124f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709:
        // BT.709 full
        g_CSC_Row0 = { 1.0000f,  0.0000f,  1.5748f};
        g_CSC_Row1 = { 1.0000f, -0.1873f, -0.4681f};
        g_CSC_Row2 = { 1.0000f,  1.8556f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020:
        // BT.2020 limited (HDR)
        g_CSC_Row0 = { 1.1644f,  0.0000f,  1.6780f};
        g_CSC_Row1 = { 1.1644f, -0.1874f, -0.6504f};
        g_CSC_Row2 = { 1.1644f,  2.1418f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020:
        // BT.2020 full (HDR)
        g_CSC_Row0 = { 1.0000f,  0.0000f,  1.4746f};
        g_CSC_Row1 = { 1.0000f, -0.1646f, -0.5713f};
        g_CSC_Row2 = { 1.0000f,  1.8814f,  0.0000f};
        break;
    case DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020:
        // BT.2020  limited (PQ/HDR)
        g_CSC_Row0 = { 1.1644f,  0.0000f,  1.6780f};
        g_CSC_Row1 = { 1.1644f, -0.1874f, -0.6504f};
        g_CSC_Row2 = { 1.1644f,  2.1418f,  0.0000f};
        break;
    default:
        // BT.601 limited
        g_CSC_Row0 = { 1.1644f,  0.0000f,  1.5960f};
        g_CSC_Row1 = { 1.1644f, -0.3918f, -0.8130f};
        g_CSC_Row2 = { 1.1644f,  2.0172f,  0.0000f};
        break;
    }

    m_RootConsts.g_CSC_Row0_x = g_CSC_Row0.x;
    m_RootConsts.g_CSC_Row0_y = g_CSC_Row0.y;
    m_RootConsts.g_CSC_Row0_z = g_CSC_Row0.z;

    m_RootConsts.g_CSC_Row1_x = g_CSC_Row1.x;
    m_RootConsts.g_CSC_Row1_y = g_CSC_Row1.y;
    m_RootConsts.g_CSC_Row1_z = g_CSC_Row1.z;

    m_RootConsts.g_CSC_Row2_x = g_CSC_Row2.x;
    m_RootConsts.g_CSC_Row2_y = g_CSC_Row2.y;
    m_RootConsts.g_CSC_Row2_z = g_CSC_Row2.z;

    if(m_IsYUV444){
        // 4:4:4 has no subsampling
        m_RootConsts.g_ScaleY = 1.0f;
        m_RootConsts.g_OffsetY = 0.0f;
        m_RootConsts.g_OffsetU = 0.0f;
        m_RootConsts.g_OffsetV = 0.0f;
    } else {
        // Color range and YUV Offset
        m_RootConsts.g_ScaleY = 1.0f;
        m_RootConsts.g_OffsetY = 0.0f;
        m_RootConsts.g_OffsetU = 0.5f;
        m_RootConsts.g_OffsetV = 0.5f;
    }
}

/**
 * \brief Create descriptor heaps
 *
 * Allocates CBV/SRV/UAV, RTV, and sampler descriptor heaps
 * with specified capacities.
 *
 * \param UINT cbvSrvUavCount Number of CBV/SRV/UAV descriptors
 * \param UINT rtvCount Number of RTV descriptors
 * \param UINT samplerCount Number of sampler descriptors
 * \return bool True if heaps created successfully
 */
bool D3D12VideoShaders::createDescriptorHeaps(UINT cbvSrvUavCount, UINT rtvCount, UINT samplerCount)
{
    if (!m_Device) return false;

    // CBV_SRV_UAV descriptor heap
    if(cbvSrvUavCount > 0){
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavdesc = {};
        cbvSrvUavdesc.NumDescriptors = cbvSrvUavCount;
        cbvSrvUavdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvSrvUavdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvSrvUavdesc.NodeMask = 0;
        m_hr = m_Device->CreateDescriptorHeap(&cbvSrvUavdesc, IID_PPV_ARGS(&m_DescriptorHeapCBV_SRV_UAV));
        if(!verifyHResult(m_hr, "m_Device->CreateDescriptorHeap(&cbvSrvUavdesc, IID_PPV_ARGS(&m_DescriptorHeapCBV_SRV_UAV));")){
            return false;
        }
        SDL_Log("SRV Descriptor Heap created successfully");
        m_DescriptorSizeCBV_SRV_UAV = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        SDL_Log("SRV Descriptor Size: %u", m_DescriptorSizeCBV_SRV_UAV);
    }

    // RTV descriptor heap (non shader-visible)
    if(rtvCount){
        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.NumDescriptors = rtvCount;
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_hr = m_Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_DescriptorHeapRTV));
        if(!verifyHResult(m_hr, "m_Device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_DescriptorHeapRTV));")){
            return false;
        }
        SDL_Log("RTV Descriptor Heap created successfully");
        m_DescriptorSizeRTV = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        SDL_Log("RTV Descriptor Size: %u", m_DescriptorSizeRTV);
    }

    // Sampler descriptor heap
    if(samplerCount){
        D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
        samplerDesc.NumDescriptors = samplerCount;
        samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_hr = m_Device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&m_DescriptorHeapSampler));
        if(!verifyHResult(m_hr, "m_Device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&m_DescriptorHeapSampler));")){
            return false;
        }
        SDL_Log("Sampler Descriptor Heap created successfully");
        m_DescriptorSizeSampler = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        SDL_Log("Sampler Descriptor Size: %u", m_DescriptorSizeSampler);
    }

    return true;
}

/**
 * \brief Create a 2D texture resource
 *
 * Creates a committed texture resource with specified dimensions,
 * format, flags, and initial state.
 *
 * \param ComPtr<ID3D12Resource>& pTexture Output texture pointer
 * \param int width Texture width
 * \param int height Texture height
 * \param DXGI_FORMAT format Texture format
 * \param D3D12_RESOURCE_FLAGS flags Resource flags
 * \param D3D12_RESOURCE_STATES textureState Initial resource state
 * \return bool True if texture created successfully
 */
bool D3D12VideoShaders::createTexture(ComPtr<ID3D12Resource>& pTexture, int width, int height, DXGI_FORMAT format,
                                      D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES textureState)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    m_hr = m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        textureState,
        nullptr,
        IID_PPV_ARGS(&pTexture)
        );
    if(!verifyHResult(m_hr, "m_Device->CreateCommittedResource(... resource)")){
        return false;
    }
    return true;
}

/**
 * \brief Create constant buffer view
 *
 * Creates a CBV for the specified resource at the given
 * descriptor heap index.
 *
 * \param ID3D12Resource* resource Constant buffer resource
 * \param UINT descriptorIndex Heap index for CBV
 * \return bool True if CBV created successfully
 */
bool D3D12VideoShaders::createCBVforResource(ID3D12Resource* resource, UINT descriptorIndex)
{
    if (!m_Device || !m_DescriptorHeapCBV_SRV_UAV || !resource) return false;
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();
    // Align to 256 bytes
    UINT sizeInBytes = static_cast<UINT>(resource->GetDesc().Width);
    cbvDesc.SizeInBytes = (sizeInBytes + 255) & ~255u;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        descriptorIndex,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateConstantBufferView(&cbvDesc, handle);
    return true;
}

/**
 * \brief Create shader resource view
 *
 * Creates an SRV for the specified texture resource with
 * format and plane slice information.
 *
 * \param ID3D12Resource* resource Texture resource
 * \param UINT descriptorIndex Heap index for SRV
 * \param DXGI_FORMAT format View format
 * \param UINT planeSlice Plane slice index
 * \return bool True if SRV created successfully
 */
bool D3D12VideoShaders::createSRVforResource(ID3D12Resource* resource, UINT descriptorIndex, DXGI_FORMAT format, UINT planeSlice)
{
    if (!m_Device || !m_DescriptorHeapCBV_SRV_UAV) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = planeSlice;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        descriptorIndex,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateShaderResourceView(resource, &srvDesc, handle);
    return true;
}

/**
 * \brief Create unordered access view
 *
 * Creates a UAV for the specified texture resource with
 * format and plane slice information.
 *
 * \param ID3D12Resource* resource Texture resource
 * \param UINT descriptorIndex Heap index for UAV
 * \param DXGI_FORMAT format View format
 * \param UINT planeSlice Plane slice index
 * \return bool True if UAV created successfully
 */
bool D3D12VideoShaders::createUAVforResource(ID3D12Resource* resource, UINT descriptorIndex, DXGI_FORMAT format, UINT planeSlice)
{
    if (!m_Device || !m_DescriptorHeapCBV_SRV_UAV) return false;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.PlaneSlice = planeSlice;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        descriptorIndex,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, handle);
    return true;
}

/**
 * \brief Create render target view
 *
 * Creates an RTV for the specified resource in the RTV heap.
 *
 * \param ID3D12Resource* resource Texture resource
 * \param UINT rtvIndex RTV heap index
 * \param DXGI_FORMAT format View format
 * \param UINT planeSlice Plane slice index
 * \return bool True if RTV created successfully
 */
bool D3D12VideoShaders::createRTVforResource(ID3D12Resource* resource, UINT rtvIndex, DXGI_FORMAT format, UINT planeSlice)
{
    if (!m_Device || !m_DescriptorHeapRTV) return false;
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.PlaneSlice = planeSlice;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_DescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), rtvIndex, m_DescriptorSizeRTV);
    m_Device->CreateRenderTargetView(resource, &rtvDesc, handle);
    return true;
}

/**
 * \brief Initialize YUV to RGB conversion pipeline
 *
 * Compiles vertex and pixel shaders, creates root signature with
 * SRV descriptors and root constants, and sets up graphics PSO.
 *
 * \return bool True if initialization succeeded
 */
bool D3D12VideoShaders::initializeCONVERT_PS()
{
    // Initialize RootConts
    initializeRootConsts();

    // 2 SRV heaps / 1 RTV heap
    createDescriptorHeaps(2, 1, 0);

    // Create SRVs for m_TextureIn
    if (!updateShaderResourceView(m_TextureIn.Get())) {
        return false;
    }

    // Create RTV descriptor for m_TextureOut at RTV index 0
    if (!createRTVforResource(m_TextureOut.Get(), 0, m_TextureOut.Get()->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createRTVforResource failed");
        return false;
    }
    SDL_Log("Created RTV for output texture");

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    ComPtr<IDxcIncludeHandler> includeHandler = new QtIncludeHandler();
    ComPtr<IDxcResult> result;

    // Vertex shader
    QFile fileVS(":/enhancer/yuv_to_rgb_ps.hlsl");
    if (!fileVS.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open yuv_to_rgb_ps.hlsl for Vertex shader");
        return false;
    }
    QByteArray hlslSourceVS = fileVS.readAll();
    fileVS.close();

    DxcBuffer sourceBufferVS = {};
    sourceBufferVS.Ptr = hlslSourceVS.data();
    sourceBufferVS.Size = hlslSourceVS.size();
    sourceBufferVS.Encoding = DXC_CP_UTF8;

    m_Args = {
        L"-T",  L"vs_6_2",
        L"-E",  L"mainVS",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    m_hr = compiler->Compile(
        &sourceBufferVS,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBufferVS)")){
        return false;
    }

    if(result){
        ComPtr<IDxcBlobEncoding> errorsBlob;
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "VS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlobVS;
    ComPtr<IDxcBlobUtf16> shaderNameVS;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlobVS), &shaderNameVS);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlobVS || shaderBlobVS->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob Vertex shader is empty!");
        return false;
    }
    SDL_Log("Vertex Shader compiled successfully");

    // Pixel shader
    QFile filePS(":/enhancer/yuv_to_rgb_ps.hlsl");
    if (!filePS.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open yuv_to_rgb_ps.hlsl for Pixel shader");
        return false;
    }
    QByteArray hlslSourcePS = filePS.readAll();
    filePS.close();

    DxcBuffer sourceBufferPS = {};
    sourceBufferPS.Ptr = hlslSourcePS.data();
    sourceBufferPS.Size = hlslSourcePS.size();
    sourceBufferPS.Encoding = DXC_CP_UTF8;

    m_Args = {
        L"-T",  L"ps_6_2",
        L"-E",  L"mainPS",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    m_hr = compiler->Compile(
        &sourceBufferPS,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBufferPS)")){
        return false;
    }

    if(result){
        ComPtr<IDxcBlobEncoding> errorsBlob;
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "PS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlobPS;
    ComPtr<IDxcBlobUtf16> shaderNamePS;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlobPS), &shaderNamePS);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlobPS || shaderBlobPS->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob Pixel shader is empty!");
        return false;
    }
    SDL_Log("Pixel Shader compiled successfully");

    // Descriptor Range
    CD3DX12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);  // 2 SRV, t0 (Y) + t1 (UV)

    // Root Parameters : descriptor table (index 0) + root constants (index 1)
    CD3DX12_ROOT_PARAMETER1 rootParameters[2] = {};
    rootParameters[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

    // Add root constants: 32 dwords mapped to register b0 in shader
    const UINT NUM_ROOT_DWORDS = 32;
    rootParameters[1].InitAsConstants(NUM_ROOT_DWORDS, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

    // Static sampler
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MipLODBias = 0;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = 0;  // s0
    samplerDesc.RegisterSpace = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Root Signature
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc,
                               rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    m_hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
                                                 D3D_ROOT_SIGNATURE_VERSION_1_1,
                                                 &signature, &error);

    if(!verifyHResult(m_hr, "D3DX12SerializeVersionedRootSignature(... rootSignatureDesc)")){
        if (error) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Root signature serialization error: %s",
                         (char*)error->GetBufferPointer());
        }
        return false;
    }

    m_hr = m_Device->CreateRootSignature(0, signature->GetBufferPointer(),
                                         signature->GetBufferSize(),
                                         IID_PPV_ARGS(&m_RootSignature));
    if(!verifyHResult(m_hr, "m_Device->CreateRootSignature(... m_RootSignature)")){
        return false;
    }
    SDL_Log("Root Signature created successfully");
    
    // Pipeline State Object
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(shaderBlobVS->GetBufferPointer(), shaderBlobVS->GetBufferSize());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(shaderBlobPS->GetBufferPointer(), shaderBlobPS->GetBufferSize());
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.InputLayout = { nullptr, 0 };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_TextureOut->GetDesc().Format;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.SampleMask = UINT_MAX;

    m_hr = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
    if(!verifyHResult(m_hr, "m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));")){
        return false;
    }
    SDL_Log("Pipeline State Object created successfully");

    SDL_Log("initializeCONVERT completed successfully");

    return true;
}

/**
 * \brief Initialize NVIDIA Image Scaling pipeline
 *
 * Compiles NIS compute shader with optimal block dimensions,
 * creates coefficient textures, and configures upscaling or sharpening.
 *
 * \param bool isUpscaling True for upscaling mode, false for sharpening
 * \return bool True if initialization succeeded
 */
bool D3D12VideoShaders::initializeNIS(bool isUpscaling)
{
    // Compile compute shader (NIS_Main.hlsl)
    QFile file(":/enhancer/NIS_Main.hlsl");
    if (!file.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open NIS_Main.hlsl");
        return false;
    }
    QByteArray hlslSource = file.readAll();
    file.close();

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = hlslSource.data();
    sourceBuffer.Size = hlslSource.size();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

    // Default on NVIDIA_Generic
    NISGPUArchitecture GPUArchitecture = NISGPUArchitecture::NVIDIA_Generic;
    if(m_AdvancedShader){
        GPUArchitecture = NISGPUArchitecture::NVIDIA_Generic_fp16;
    } else if(m_VideoEnhancement->isVendorAMD()){
        GPUArchitecture = NISGPUArchitecture::AMD_Generic;
    } else if(m_VideoEnhancement->isVendorIntel()){
        GPUArchitecture = NISGPUArchitecture::Intel_Generic;
    } else if(m_VideoEnhancement->isVendorNVIDIA()){
        GPUArchitecture = NISGPUArchitecture::NVIDIA_Generic;
    }
    
    NISOptimizer opt(true, GPUArchitecture);
    uint32_t blockWidth = opt.GetOptimalBlockWidth();
    uint32_t blockHeight = opt.GetOptimalBlockHeight();
    uint32_t threadGroupSize = opt.GetOptimalThreadGroupSize();

    m_DispatchX = static_cast<UINT>(std::ceil(m_OutWidth / float(blockWidth)));
    m_DispatchY = static_cast<UINT>(std::ceil(m_OutHeight / float(blockHeight)));

    m_NIS_SCALER = std::wstring(L"NIS_SCALER=") + (isUpscaling ? L"1" : L"0");
    m_NIS_HDR_MODE = std::wstring(L"NIS_HDR_MODE=") + (m_IsHDR ? L"2" : L"0");
    m_NIS_BLOCK_WIDTH = std::wstring(L"NIS_BLOCK_WIDTH=") + std::to_wstring(blockWidth);
    m_NIS_BLOCK_HEIGHT = std::wstring(L"NIS_BLOCK_HEIGHT=") + std::to_wstring(blockHeight);
    m_NIS_THREAD_GROUP_SIZE = std::wstring(L"NIS_THREAD_GROUP_SIZE=") + std::to_wstring(threadGroupSize);

    m_Args = {
        L"-T",  L"cs_6_2",
        L"-E",  L"main",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
        L"-D",  m_NIS_SCALER.c_str(),
        L"-D",  m_NIS_HDR_MODE.c_str(),
        L"-D",  m_NIS_BLOCK_WIDTH.c_str(),
        L"-D",  m_NIS_BLOCK_HEIGHT.c_str(),
        L"-D",  m_NIS_THREAD_GROUP_SIZE.c_str(),
        L"-D",  L"NIS_HLSL_6_2=1",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-D");
        m_Args.push_back(L"NIS_USE_HALF_PRECISION=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    ComPtr<IDxcIncludeHandler> includeHandler = new QtIncludeHandler();

    ComPtr<IDxcResult> result;
    m_hr = compiler->Compile(
        &sourceBuffer,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBuffer)")){
        return false;
    }

    if(result){
        ComPtr<IDxcBlobEncoding> errorsBlob;
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<IDxcBlobUtf16> shaderName;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), &shaderName);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlob || shaderBlob->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob is empty!");
        return false;
    }
    SDL_Log("Compute Shader compiled successfully");

    // Create buffer m_stagingBuffer
    NISCreateBuffer(sizeof(NISConfig), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, &m_StagingBuffer);

    // Create buffer m_constatBuffer
    NISCreateBuffer(sizeof(NISConfig), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, &m_ConstatBuffer);

    // Define root table layout
    constexpr uint32_t nParams = 6;
    CD3DX12_DESCRIPTOR_RANGE descriptorRange[nParams] = {};
    descriptorRange[0] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
    descriptorRange[1] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    descriptorRange[2] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    descriptorRange[3] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    descriptorRange[4] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    descriptorRange[5] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

    CD3DX12_ROOT_PARAMETER rootParams[nParams] = {};
    rootParams[0].InitAsDescriptorTable(1, &descriptorRange[0]);
    rootParams[1].InitAsDescriptorTable(1, &descriptorRange[1]);
    rootParams[2].InitAsDescriptorTable(1, &descriptorRange[2]);
    rootParams[3].InitAsDescriptorTable(1, &descriptorRange[3]);
    rootParams[4].InitAsDescriptorTable(1, &descriptorRange[4]);
    rootParams[5].InitAsDescriptorTable(1, &descriptorRange[5]);

    // Create the root signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.NumParameters = nParams;
    rootSignatureDesc.pParameters = rootParams;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> serializedSignature;
    ComPtr<ID3DBlob> error;
    m_hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedSignature,
        &error);
    if(!verifyHResult(m_hr, "D3D12SerializeRootSignature(... rootSignatureDesc)")){
        if (error) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Root signature serialization error: %s",
                         (char*)error->GetBufferPointer());
        }
        return false;
    }
    m_hr = m_Device->CreateRootSignature(
        0,
        serializedSignature->GetBufferPointer(),
        serializedSignature->GetBufferSize(),
        IID_PPV_ARGS(&m_RootSignature));
    m_RootSignature->SetName(L"NVScaler");

    // Create compute pipeline state
    D3D12_COMPUTE_PIPELINE_STATE_DESC descComputePSO = {};
    descComputePSO.pRootSignature = m_RootSignature.Get();
    descComputePSO.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
    descComputePSO.CS.BytecodeLength = shaderBlob->GetBufferSize();

    m_hr = m_Device->CreateComputePipelineState(&descComputePSO, IID_PPV_ARGS(&m_PipelineState));
    if(!verifyHResult(m_hr, "m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));")){
        return false;
    }
    m_PipelineState->SetName(L"NVScaler Compute PSO");

    m_RowPitch = kFilterSize * 4;
    const int rowPitchAligned = (m_RowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    const int coefSize = rowPitchAligned * kPhaseCount;

    // Create buffer texture Scaler
    NISCreateBuffer(coefSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, &m_CoefScalerUpload);
    NISCreateTexture2D(kFilterSize / 4, kPhaseCount,
                       DXGI_FORMAT_R16G16B16A16_FLOAT,
                       D3D12_RESOURCE_STATE_COMMON, &m_CoefScaler);
    NISCreateAlignedCoefficients(
        (uint16_t*)coef_scale_fp16,
        m_CoefScalerHost,
        rowPitchAligned);

    // Create buffer texture USM
    NISCreateBuffer(coefSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, &m_CoefUSMUpload);
    NISCreateTexture2D(kFilterSize / 4, kPhaseCount,
                       DXGI_FORMAT_R16G16B16A16_FLOAT,
                       D3D12_RESOURCE_STATE_COMMON, &m_CoefUSM);
    NISCreateAlignedCoefficients(
        (uint16_t*)coef_usm_fp16,
        m_CoefUSMHost,
        rowPitchAligned);

    // Create: 6 SRV descriptor heaps / 1 Sampler descriptor heap
    createDescriptorHeaps(6, 0, 1);

    // Sampler
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleSampler(
        m_DescriptorHeapSampler->GetCPUDescriptorHandleForHeapStart(),
        0,
        m_DescriptorSizeSampler);
    m_Device->CreateSampler(&samplerDesc, handleSampler);

    // CBV
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_ConstatBuffer.Get()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = sizeof(NISConfig);
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleCBV(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        1,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateConstantBufferView(&cbvDesc, handleCBV);

    // TextureIn
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleTextureIn(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        2,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_TextureIn.Get(), nullptr, handleTextureIn);

    // TextureOut
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleTextureOut(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        3,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_TextureOut.Get(), nullptr, handleTextureOut);

    // CoefScaler
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleCoefScaler(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        4,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_CoefScaler.Get(), nullptr, handleCoefScaler);

    // CoefUSM
    CD3DX12_CPU_DESCRIPTOR_HANDLE handleCoefUSM(
        m_DescriptorHeapCBV_SRV_UAV->GetCPUDescriptorHandleForHeapStart(),
        5,
        m_DescriptorSizeCBV_SRV_UAV);
    m_Device->CreateShaderResourceView(m_CoefUSM.Get(), nullptr, handleCoefUSM);

    // Initialize NVScaler
    if(isUpscaling){
        NVScalerUpdateConfig(m_ConfigNIS, 0.15f,
                             0, 0, m_InWidth, m_InHeight, m_InWidth, m_InHeight,
                             0, 0, m_OutWidth, m_OutHeight, m_OutWidth, m_OutHeight,
                             m_IsHDR ? NISHDRMode::PQ : NISHDRMode::None);
    } else {
        NVSharpenUpdateConfig(m_ConfigNIS, 0.35f,
                              0, 0, m_InWidth, m_InHeight, m_InWidth, m_InHeight,
                              0, 0, m_IsHDR ? NISHDRMode::PQ : NISHDRMode::None);
    }

    // Copy m_config (data) to m_constatBuffer (Constant Buffer)
    NISUploadBufferData(&m_ConfigNIS, sizeof(NISConfig), m_ConstatBuffer.Get(), m_StagingBuffer.Get());


    // Copy m_coefScalerHost (data) to m_coefScaler (Texture)
    NISUploadTextureData((void*)m_CoefScalerHost.data(), sizeof(m_CoefScalerHost[0]) * uint32_t(m_CoefScalerHost.size()),
                         m_RowPitch, m_CoefScaler.Get(), m_CoefScalerUpload.Get());

    // Copy m_coefUSMHost (data) to m_coefUSM (Texture)
    NISUploadTextureData((void*)m_CoefUSMHost.data(), sizeof(m_CoefUSMHost[0]) * uint32_t(m_CoefUSMHost.size()),
                         m_RowPitch, m_CoefUSM.Get(), m_CoefUSMUpload.Get());

    return true;
}

/**
 * \brief Initialize FidelityFX Super Resolution 1.0 pipeline
 *
 * Compiles EASU (upscaler) and RCAS (sharpener) compute shaders,
 * creates intermediate texture, and sets up two-pass pipeline.
 *
 * \return bool True if initialization succeeded
 */
bool D3D12VideoShaders::initializeFSR1()
{
    // FSR1 Documentation implementation:
    // https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/docs/FidelityFX-FSR-Overview-Integration.pdf

    // 2 SRV heaps + 2 UAV heaps
    createDescriptorHeaps(4, 0, 0);

    // Intermediate Texture from EASU to RCAS
    createTexture(m_TextureInterm,
                  m_OutWidth,
                  m_OutHeight,
                  m_TextureOut->GetDesc().Format,
                  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // Create SRV for m_TextureIn (RGB)
    if (!updateShaderResourceView(m_TextureIn.Get())) {
        return false;
    }

    // Create SRV descriptor for m_TextureInterm
    if (!createSRVforResource(m_TextureInterm.Get(), 1, m_TextureInterm->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createSRVforResource failed");
        return false;
    }
    SDL_Log("Created SRV for intermediate texture");

    // Create UAV descriptor for m_TextureInterm
    if (!createUAVforResource(m_TextureInterm.Get(), 2, m_TextureInterm->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createUAVforResource failed");
        return false;
    }
    SDL_Log("Created UAV for intermediate texture");

    // Create UAV descriptor for m_TextureOut
    if (!createUAVforResource(m_TextureOut.Get(), 3, m_TextureOut->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createUAVforResource failed");
        return false;
    }
    SDL_Log("Created UAV for output texture");

    m_DispatchX = static_cast<UINT>(std::ceil(m_OutWidth / float(16)));
    m_DispatchY = static_cast<UINT>(std::ceil(m_OutHeight / float(16)));

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    ComPtr<IDxcIncludeHandler> includeHandler = new QtIncludeHandler();
    ComPtr<IDxcResult> result;
    ComPtr<IDxcBlobEncoding> errorsBlob;

    // FSR1 EASU (Upscaler)
    FsrEasuCon(
        reinterpret_cast<AU1*>(m_EASUConstants.const0),
        reinterpret_cast<AU1*>(m_EASUConstants.const1),
        reinterpret_cast<AU1*>(m_EASUConstants.const2),
        reinterpret_cast<AU1*>(m_EASUConstants.const3),
        static_cast<AF1>(m_InWidth),
        static_cast<AF1>(m_InHeight),
        static_cast<AF1>(m_InWidth),
        static_cast<AF1>(m_InHeight),
        static_cast<AF1>(m_OutWidth),
        static_cast<AF1>(m_OutHeight));

    // EASU Shader Preparation
    // Compile compute shader (FSR_Pass.hlsl)
    QFile file(":/enhancer/FSR_Pass.hlsl");
    if (!file.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open FSR_Pass.hlsl");
        return false;
    }
    QByteArray hlslSource = file.readAll();
    file.close();

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = hlslSource.data();
    sourceBuffer.Size = hlslSource.size();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    m_Args = {
        L"-T",  L"cs_6_2",
        L"-E",  L"mainCS",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
        L"-HV", L"2018",
        L"-D", L"APPLY_EASU=1",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    m_hr = compiler->Compile(
        &sourceBuffer,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBuffer)")){
        return false;
    }

    if(result){
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlobEASU;
    ComPtr<IDxcBlobUtf16> shaderNameEASU;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlobEASU), &shaderNameEASU);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlobEASU || shaderBlobEASU->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob is empty!");
        return false;
    }
    SDL_Log("FSR1 EASU Shader compiled successfully");

    // FSR1 RCAS (Sharpener)
    AF1 sharpeness = 0.75f;
    FsrRcasCon(
        reinterpret_cast<AU1*>(m_RCASConstants.const0),
        sharpeness);

    m_Args = {
        L"-T",  L"cs_6_2",
        L"-E",  L"mainCS",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
        L"-HV", L"2018",
        L"-D", L"APPLY_RCAS=1",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    m_hr = compiler->Compile(
        &sourceBuffer,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBuffer)")){
        return false;
    }

    if(result){
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlobRCAS;
    ComPtr<IDxcBlobUtf16> shaderNameRCAS;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlobRCAS), &shaderNameRCAS);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlobRCAS || shaderBlobRCAS->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob is empty!");
        return false;
    }
    SDL_Log("FSR1 RCAS Shader compiled successfully");


    // Descriptor Range
    CD3DX12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // 1 SRV, t0 (RGB)

    // Descriptor Range
    CD3DX12_DESCRIPTOR_RANGE1 uavRange = {};
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 UAV, u0 (RGB)

    // Root Parameters : descriptor table (index 0) + root constants (index 1)
    CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};
    rootParameters[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);

    // Add root constants: 32 dwords mapped to register b0 in shader
    const UINT NUM_ROOT_DWORDS = 32;
    rootParameters[2].InitAsConstants(NUM_ROOT_DWORDS, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

    // Sampler
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MipLODBias = 0;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = 0;  // s0
    samplerDesc.RegisterSpace = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root Signature
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc,
                               rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    m_hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
                                                 D3D_ROOT_SIGNATURE_VERSION_1_1,
                                                 &signature, &error);

    if(!verifyHResult(m_hr, "D3DX12SerializeVersionedRootSignature(... rootSignatureDesc)")){
        if (error) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Root signature serialization error: %s",
                         (char*)error->GetBufferPointer());
        }
        return false;
    }

    m_hr = m_Device->CreateRootSignature(0, signature->GetBufferPointer(),
                                         signature->GetBufferSize(),
                                         IID_PPV_ARGS(&m_RootSignature));
    if(!verifyHResult(m_hr, "m_Device->CreateRootSignature(... m_RootSignature)")){
        return false;
    }
    SDL_Log("Root Signature created successfully");

    // --- Create Compute PSO ---

    // FSR1 EASU (Upscaler)
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDescEASU = {};
    psoDescEASU.pRootSignature = m_RootSignature.Get();
    psoDescEASU.CS = CD3DX12_SHADER_BYTECODE(shaderBlobEASU->GetBufferPointer(), shaderBlobEASU->GetBufferSize());
    psoDescEASU.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    m_hr = m_Device->CreateComputePipelineState(&psoDescEASU, IID_PPV_ARGS(&m_PipelineStateEASU));
    if(!verifyHResult(m_hr, "m_Device->CreateComputePipelineState(&psoDescEASU, IID_PPV_ARGS(&m_PipelineStateEASU));")){
        return false;
    }

    // FSR1 RCAS (Sharpener)
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDescRCAS = {};
    psoDescRCAS.pRootSignature = m_RootSignature.Get();
    psoDescRCAS.CS = CD3DX12_SHADER_BYTECODE(shaderBlobRCAS->GetBufferPointer(), shaderBlobRCAS->GetBufferSize());
    psoDescRCAS.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    m_hr = m_Device->CreateComputePipelineState(&psoDescRCAS, IID_PPV_ARGS(&m_PipelineStateRCAS));
    if(!verifyHResult(m_hr, "m_Device->CreateComputePipelineState(&psoDescRCAS, IID_PPV_ARGS(&m_PipelineStateRCAS));")){
        return false;
    }

    return true;
}

/**
 * \brief Initialize RCAS sharpening pipeline
 *
 * Compiles RCAS compute shader and creates pipeline for
 * standalone sharpening without upscaling.
 *
 * \return bool True if initialization succeeded
 */
bool D3D12VideoShaders::initializeRCAS()
{
    // FSR1 Documentation implementation:
    // https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/docs/FidelityFX-FSR-Overview-Integration.pdf

    // 1 SRV heap + 1 UAV heap
    createDescriptorHeaps(2, 0, 0);

    // Create SRV for m_TextureIn (RGB)
    if (!updateShaderResourceView(m_TextureIn.Get())) {
        return false;
    }

    // Create UAV descriptor for m_TextureOut
    if (!createUAVforResource(m_TextureOut.Get(), 1, m_TextureOut->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createUAVforResource failed");
        return false;
    }
    SDL_Log("Created UAV for output texture");

    m_DispatchX = static_cast<UINT>(std::ceil(m_OutWidth / float(16)));
    m_DispatchY = static_cast<UINT>(std::ceil(m_OutHeight / float(16)));

    ComPtr<IDxcCompiler3> compiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    ComPtr<IDxcIncludeHandler> includeHandler = new QtIncludeHandler();
    ComPtr<IDxcResult> result;
    ComPtr<IDxcBlobEncoding> errorsBlob;

    // FSR1 RCAS (Sharpener)
    AF1 sharpeness = 0.6f;
    FsrRcasCon(
        reinterpret_cast<AU1*>(m_RCASConstants.const0),
        sharpeness);

    // RCAS Shader Preparation
    // Compile compute shader (FSR_Pass.hlsl)
    QFile file(":/enhancer/FSR_Pass.hlsl");
    if (!file.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open FSR_Pass.hlsl");
        return false;
    }
    QByteArray hlslSource = file.readAll();
    file.close();

    DxcBuffer sourceBuffer = {};
    sourceBuffer.Ptr = hlslSource.data();
    sourceBuffer.Size = hlslSource.size();
    sourceBuffer.Encoding = DXC_CP_UTF8;

    m_Args = {
        L"-T",  L"cs_6_2",
        L"-E",  L"mainCS",
        L"-O3",
        L"-Qstrip_reflect",
        L"-Qstrip_debug",
        L"-HV", L"2018",
        L"-D", L"APPLY_RCAS=1",
    };
    if(m_AdvancedShader){
        m_Args.push_back(L"-D");
        m_Args.push_back(L"ADVANCED_SHADER=1");
        m_Args.push_back(L"-enable-16bit-types");
    }

    m_hr = compiler->Compile(
        &sourceBuffer,
        m_Args.data(),
        (UINT32)m_Args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result));
    if(!verifyHResult(m_hr, "compiler->Compile(... sourceBuffer)")){
        return false;
    }

    if(result){
        m_hr = result->GetErrorBuffer(&errorsBlob);
        if(!verifyHResult(m_hr, "result->GetErrorBuffer(&errorsBlob)")){
            if (errorsBlob) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CS compile error: %s",
                             (char*)errorsBlob->GetBufferPointer());
            }
            return false;
        }
    }

    ComPtr<IDxcBlob> shaderBlobRCAS;
    ComPtr<IDxcBlobUtf16> shaderNameRCAS;
    m_hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlobRCAS), &shaderNameRCAS);
    if (!verifyHResult(m_hr, "result->GetOutput(DXC_OUT_OBJECT, ...)")) {
        return false;
    }
    if (!shaderBlobRCAS || shaderBlobRCAS->GetBufferSize() == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader blob is empty!");
        return false;
    }
    SDL_Log("FSR1 RCAS Shader compiled successfully");


    // Descriptor Range
    CD3DX12_DESCRIPTOR_RANGE1 srvRange = {};
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);  // 1 SRV, t0 (RGB)

    // Descriptor Range
    CD3DX12_DESCRIPTOR_RANGE1 uavRange = {};
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 UAV, u0 (RGB)

    // Root Parameters : descriptor table (index 0) + root constants (index 1)
    CD3DX12_ROOT_PARAMETER1 rootParameters[3] = {};
    rootParameters[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
    rootParameters[1].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);

    // Add root constants: 32 dwords mapped to register b0 in shader
    const UINT NUM_ROOT_DWORDS = 32;
    rootParameters[2].InitAsConstants(NUM_ROOT_DWORDS, 0, 0, D3D12_SHADER_VISIBILITY_ALL);

    // Sampler
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MipLODBias = 0;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = 0;  // s0
    samplerDesc.RegisterSpace = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root Signature
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &samplerDesc,
                               rootSignatureFlags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    m_hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc,
                                                 D3D_ROOT_SIGNATURE_VERSION_1_1,
                                                 &signature, &error);

    if(!verifyHResult(m_hr, "D3DX12SerializeVersionedRootSignature(... rootSignatureDesc)")){
        if (error) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Root signature serialization error: %s",
                         (char*)error->GetBufferPointer());
        }
        return false;
    }

    m_hr = m_Device->CreateRootSignature(0, signature->GetBufferPointer(),
                                         signature->GetBufferSize(),
                                         IID_PPV_ARGS(&m_RootSignature));
    if(!verifyHResult(m_hr, "m_Device->CreateRootSignature(... m_RootSignature)")){
        return false;
    }
    SDL_Log("Root Signature created successfully");

    // --- Create Compute PSO ---
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDescRCAS = {};
    psoDescRCAS.pRootSignature = m_RootSignature.Get();
    psoDescRCAS.CS = CD3DX12_SHADER_BYTECODE(shaderBlobRCAS->GetBufferPointer(), shaderBlobRCAS->GetBufferSize());
    psoDescRCAS.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    m_hr = m_Device->CreateComputePipelineState(&psoDescRCAS, IID_PPV_ARGS(&m_PipelineStateRCAS));
    if(!verifyHResult(m_hr, "m_Device->CreateComputePipelineState(&psoDescRCAS, IID_PPV_ARGS(&m_PipelineStateRCAS));")){
        return false;
    }


    return true;
}

/**
 * \brief Initialize simple copy pipeline
 *
 * Creates a basic compute shader pipeline for copying RGB
 * textures without enhancement.
 *
 * \return bool True if initialization succeeded
 */
bool D3D12VideoShaders::initializeCOPY()
{
    // Works only for RGB
    if (m_isYUV) return false;

    // 1 SRV heap + 1 UAV heap
    createDescriptorHeaps(2, 0, 0);

    // Create SRV for m_TextureIn
    if (!updateShaderResourceView(m_TextureIn.Get())) {
        return false;
    }

    // Create UAV for m_TextureOut
    if (!createUAVforResource(m_TextureOut.Get(), 1, m_TextureOut->GetDesc().Format)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "createUAVforResource failed");
        return false;
    }

    // --- Root signature with descriptor table (t0 + u0) + root constants (4 DWORDs) ---
    CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};
    // range 0 = SRV range with 1 descriptor starting at t0
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // numDescriptors=1, baseShaderRegister=0 (t0)
    // range 1 = UAV range with 1 descriptor starting at u0
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // numDescriptors=1, baseShaderRegister=0 (u0)

    CD3DX12_ROOT_PARAMETER rootParams[2] = {};
    // Table 0 = SRV range
    rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
    // Table 1 = UAV range
    rootParams[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);


    // Compile compute shader (copy_cs.hlsl)
    QFile file(":/enhancer/copy_cs.hlsl");
    if (!file.open(QIODevice::ReadOnly)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot open copy_cs.hlsl");
        return false;
    }
    QByteArray hlslSource = file.readAll();
    file.close();

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;
    m_hr = D3DCompile(hlslSource.constData(), hlslSource.size(), "copy_cs.hlsl", nullptr, nullptr, "mainCS", "cs_5_0",
                      D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &shaderBlob, &errorBlob);
    if(!verifyHResult(m_hr, "D3DCompile(... copy_cs.hlsl)")){
        if (errorBlob) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CS compile error: %s",
                         (char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    // create root signature
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> rsBlob;
    ComPtr<ID3DBlob> rsErr;
    m_hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErr);
    if(!verifyHResult(m_hr, "D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErr);")){
        if (rsErr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "RootSig serialize error: %s", (char*)rsErr->GetBufferPointer());
        }
        return false;
    }
    m_hr = m_Device->CreateRootSignature(
        0,
        rsBlob->GetBufferPointer(),
        rsBlob->GetBufferSize(),
        IID_PPV_ARGS(&m_RootSignature)
        );
    if(!verifyHResult(m_hr, "m_Device->CreateRootSignature(... m_RootSignature)")){
        return false;
    }

    // --- Create Compute PSO ---
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
    psoDesc.CS.BytecodeLength  = shaderBlob->GetBufferSize();
    psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    m_hr = m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
    if(!verifyHResult(m_hr, "m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));")){
        return false;
    }

    m_DispatchX = static_cast<UINT>(std::ceil(m_OutWidth / float(16)));
    m_DispatchY = static_cast<UINT>(std::ceil(m_OutHeight / float(16)));

    return true;
}

/**
 * \brief Record draw commands for selected enhancer
 *
 * Transitions resources to required states and dispatches
 * the appropriate shader passes for the current enhancer.
 *
 * \param D3D12_RESOURCE_STATES inputTextureStateIn Initial state of input texture
 * \param D3D12_RESOURCE_STATES inputTextureStateOut Final state of input texture
 * \param D3D12_RESOURCE_STATES outputTextureStateIn Initial state of output texture
 * \param D3D12_RESOURCE_STATES outputTextureStateOut Final state of output texture
 */
void D3D12VideoShaders::draw(
    D3D12_RESOURCE_STATES inputTextureStateIn,
    D3D12_RESOURCE_STATES inputTextureStateOut,
    D3D12_RESOURCE_STATES outputTextureStateIn,
    D3D12_RESOURCE_STATES outputTextureStateOut
    )
{
    if (!m_GraphicsCommandList) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No command list for draw()");
        return;
    }

    m_InputTextureStateIn = inputTextureStateIn;
    m_InputTextureStateOut = inputTextureStateOut;
    m_OutputTextureStateIn = outputTextureStateIn;
    m_OutputTextureStateOut = outputTextureStateOut;

    switch (m_Enhancer) {
    case Enhancer::CONVERT_PS:
        applyCONVERT_PS();
        break;
    case Enhancer::NIS:
        applyNIS();
        break;
    case Enhancer::NIS_SHARPENER:
        applyNIS();
        break;
    case Enhancer::FSR1:
        applyFSR1();
        break;
    case Enhancer::RCAS:
        applyRCAS();
        break;
    case Enhancer::COPY:
        applyCOPY();
        break;
    default:
        break;
    }
}

/**
 * \brief Apply YUV to RGB conversion
 *
 * Transitions resources, sets graphics pipeline, binds descriptors,
 * and draws fullscreen triangle to convert YUV input to RGB output.
 *
 * \return bool True if conversion applied successfully
 */
bool D3D12VideoShaders::applyCONVERT_PS()
{
    if (!m_GraphicsCommandList || !m_PipelineState) return false;

    CD3DX12_RESOURCE_BARRIER barriers[2];
    UINT NbrBarriers;

    NbrBarriers = 0;
    if(m_InputTextureStateIn != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            m_InputTextureStateIn,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateIn != D3D12_RESOURCE_STATE_RENDER_TARGET){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            m_OutputTextureStateIn,
            D3D12_RESOURCE_STATE_RENDER_TARGET
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    m_GraphicsCommandList->SetPipelineState(m_PipelineState.Get());
    m_GraphicsCommandList->SetGraphicsRootSignature(m_RootSignature.Get());

    // Set descriptor heap and root signature/PSO
    ID3D12DescriptorHeap* heaps[] = { m_DescriptorHeapCBV_SRV_UAV.Get() };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_GraphicsCommandList->SetGraphicsRootDescriptorTable(
        0,
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart()
        );

    m_GraphicsCommandList->RSSetViewports(1, &m_Viewport);
    m_GraphicsCommandList->RSSetScissorRects(1, &m_ScissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_DescriptorHeapRTV->GetCPUDescriptorHandleForHeapStart());

    m_GraphicsCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Clear the texture in black
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_GraphicsCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Root Constants
    m_GraphicsCommandList->SetGraphicsRoot32BitConstants(1, 32, &m_RootConsts, 0);

    // Set primitive topology (triangle) and draw fullscreen triangle with SV_VertexID
    m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_GraphicsCommandList->DrawInstanced(3, 1, 0, 0);

    NbrBarriers = 0;
    if(m_InputTextureStateOut != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            m_InputTextureStateOut
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateOut != D3D12_RESOURCE_STATE_RENDER_TARGET){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            m_OutputTextureStateOut
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    return true;
}

/**
 * \brief Apply NVIDIA Image Scaling
 *
 * Transitions resources, sets compute pipeline, binds descriptors
 * and constant buffers, and dispatches NIS compute shader.
 *
 * \return bool True if NIS applied successfully
 */
bool D3D12VideoShaders::applyNIS()
{
    if (!m_GraphicsCommandList || !m_PipelineState) return false;

    CD3DX12_RESOURCE_BARRIER barriers[2];
    UINT NbrBarriers;

    NbrBarriers = 0;
    if(m_InputTextureStateIn != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            m_InputTextureStateIn,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateIn != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            m_OutputTextureStateIn,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    m_GraphicsCommandList->SetPipelineState(m_PipelineState.Get());
    m_GraphicsCommandList->SetComputeRootSignature(m_RootSignature.Get());

    // Set descriptor heap and root signature/PSO
    ID3D12DescriptorHeap* heaps[] = {
        m_DescriptorHeapCBV_SRV_UAV.Get(),  // SRV/UAV/CBV
        m_DescriptorHeapSampler.Get()       // Samplers
    };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Sampler
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSampler = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapSampler->GetGPUDescriptorHandleForHeapStart(),
        0,
        m_DescriptorSizeSampler
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(0, gpuSampler);

    // CBV
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuCBV = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        1,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(1, gpuCBV);

    // SRV (Input texture)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        2,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(2, gpuSRV);

    // UAV (Output)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        3,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(3, gpuUAV);

    // SRV (Coef Scaler)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRVScaler = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        4,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(4, gpuSRVScaler);

    // SRV (Coef USM)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuSRVUSM = CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        5,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(5, gpuSRVUSM);

    // Dispatch
    m_GraphicsCommandList->Dispatch(m_DispatchX, m_DispatchY, 1);

    NbrBarriers = 0;
    if(m_InputTextureStateOut != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            m_InputTextureStateOut
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateOut != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            m_OutputTextureStateOut
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    return true;
}

/**
 * \brief Apply FSR1 two-pass enhancement
 *
 * Executes EASU upscaling pass to intermediate texture,
 * then RCAS sharpening pass to final output.
 *
 * \return bool True if FSR1 applied successfully
 */
bool D3D12VideoShaders::applyFSR1()
{
    if (!m_GraphicsCommandList || !m_PipelineStateEASU || !m_PipelineStateRCAS) return false;

    CD3DX12_RESOURCE_BARRIER barriers[2];
    UINT NbrBarriers;


    // Set descriptor heap and root signature/PSO
    ID3D12DescriptorHeap* heaps[] = { m_DescriptorHeapCBV_SRV_UAV.Get() };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    NbrBarriers = 0;
    if(m_InputTextureStateIn != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            m_InputTextureStateIn,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateIn != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            m_OutputTextureStateIn,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    // ===============================
    // PASS 1 : EASU
    // ===============================

    {
        m_GraphicsCommandList->SetPipelineState(m_PipelineStateEASU.Get());
        m_GraphicsCommandList->SetComputeRootSignature(m_RootSignature.Get());

        // Set SRV descriptor table (descriptor 0)
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSRVs(
            m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
            0,
            m_DescriptorSizeCBV_SRV_UAV
            );
        m_GraphicsCommandList->SetComputeRootDescriptorTable(0, gpuHandleSRVs);

        // Set UAV descriptor table (descriptor 1)
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleUAV(
            m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
            1,
            m_DescriptorSizeCBV_SRV_UAV);
        m_GraphicsCommandList->SetComputeRootDescriptorTable(1, gpuHandleUAV);

        // Root Constants
        m_GraphicsCommandList->SetComputeRoot32BitConstants(2, 32, &m_EASUConstants, 0);

        // Dispatch
        m_GraphicsCommandList->Dispatch(m_DispatchX, m_DispatchY, 1);
    }

    // ===============================
    // PASS 2 : RCAS
    // ===============================

    {
        m_GraphicsCommandList->SetPipelineState(m_PipelineStateRCAS.Get());
        m_GraphicsCommandList->SetComputeRootSignature(m_RootSignature.Get());

        // Set SRV descriptor table (descriptor 2)
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSRVs(
            m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
            2,
            m_DescriptorSizeCBV_SRV_UAV
            );
        m_GraphicsCommandList->SetComputeRootDescriptorTable(0, gpuHandleSRVs);

        // Set UAV descriptor table (descriptor 3)
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleUAV(
            m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
            3,
            m_DescriptorSizeCBV_SRV_UAV);
        m_GraphicsCommandList->SetComputeRootDescriptorTable(1, gpuHandleUAV);

        // Root Constants
        m_GraphicsCommandList->SetComputeRoot32BitConstants(2, 32, &m_RCASConstants, 0);

        // Dispatch
        m_GraphicsCommandList->Dispatch(m_DispatchX, m_DispatchY, 1);
    }

    NbrBarriers = 0;
    if(m_InputTextureStateOut != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            m_InputTextureStateOut
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateOut != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            m_OutputTextureStateOut
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    return true;
}

/**
 * \brief Apply RCAS sharpening
 *
 * Transitions resources, sets compute pipeline, and dispatches
 * RCAS shader for standalone sharpening.
 *
 * \return bool True if RCAS applied successfully
 */
bool D3D12VideoShaders::applyRCAS()
{
    if (!m_GraphicsCommandList || !m_PipelineStateRCAS) return false;

    CD3DX12_RESOURCE_BARRIER barriers[2];
    UINT NbrBarriers;


    // Set descriptor heap and root signature/PSO
    ID3D12DescriptorHeap* heaps[] = { m_DescriptorHeapCBV_SRV_UAV.Get() };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    NbrBarriers = 0;
    if(m_InputTextureStateIn != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            m_InputTextureStateIn,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateIn != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            m_OutputTextureStateIn,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    m_GraphicsCommandList->SetPipelineState(m_PipelineStateRCAS.Get());
    m_GraphicsCommandList->SetComputeRootSignature(m_RootSignature.Get());

    // Set SRV descriptor table (descriptor 0)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSRVs(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        0,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(0, gpuHandleSRVs);

    // Set UAV descriptor table (descriptor 1)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleUAV(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        1,
        m_DescriptorSizeCBV_SRV_UAV);
    m_GraphicsCommandList->SetComputeRootDescriptorTable(1, gpuHandleUAV);

    // Root Constants
    m_GraphicsCommandList->SetComputeRoot32BitConstants(2, 32, &m_RCASConstants, 0);

    // Dispatch
    m_GraphicsCommandList->Dispatch(m_DispatchX, m_DispatchY, 1);

    NbrBarriers = 0;
    if(m_InputTextureStateOut != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            m_InputTextureStateOut
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateOut != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            m_OutputTextureStateOut
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    return true;
}

/**
 * \brief Apply simple texture copy
 *
 * Transitions resources and dispatches compute shader to
 * copy input texture to output without modification.
 *
 * \return bool True if copy applied successfully
 */
bool D3D12VideoShaders::applyCOPY()
{
    if (!m_GraphicsCommandList || !m_PipelineState) return false;

    CD3DX12_RESOURCE_BARRIER barriers[2];
    UINT NbrBarriers;

    NbrBarriers = 0;
    if(m_InputTextureStateIn != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            m_InputTextureStateIn,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateIn != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            m_OutputTextureStateIn,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    // Guarantee descriptor heap is set
    ID3D12DescriptorHeap* heaps[] = { m_DescriptorHeapCBV_SRV_UAV.Get() };
    m_GraphicsCommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Bind compute root signature & PSO
    m_GraphicsCommandList->SetComputeRootSignature(m_RootSignature.Get());
    m_GraphicsCommandList->SetPipelineState(m_PipelineState.Get());

    // Set SRV descriptor table (descriptor 0)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleSRVs(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        0,
        m_DescriptorSizeCBV_SRV_UAV
        );
    m_GraphicsCommandList->SetComputeRootDescriptorTable(0, gpuHandleSRVs);

    // Set UAV descriptor table (descriptor 1)
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandleUAV(
        m_DescriptorHeapCBV_SRV_UAV->GetGPUDescriptorHandleForHeapStart(),
        1,
        m_DescriptorSizeCBV_SRV_UAV);
    m_GraphicsCommandList->SetComputeRootDescriptorTable(1, gpuHandleUAV);

    // Dispatch
    m_GraphicsCommandList->Dispatch(m_DispatchX, m_DispatchY, 1);

    NbrBarriers = 0;
    if(m_InputTextureStateOut != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureIn.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            m_InputTextureStateOut
            );
        NbrBarriers++;
    }
    if(m_OutputTextureStateOut != D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        barriers[NbrBarriers] = CD3DX12_RESOURCE_BARRIER::Transition(
            m_TextureOut.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            m_OutputTextureStateOut
            );
        NbrBarriers++;
    }
    if(NbrBarriers > 0){
        m_GraphicsCommandList->ResourceBarrier(NbrBarriers, barriers);
    }

    return true;
}


// Following methods are specific to NIS

/**
 * \brief Create 2D texture for NIS
 *
 * Helper method to create a default heap texture with
 * UAV flag for NIS coefficient storage.
 *
 * \param uint32_t width Texture width
 * \param uint32_t height Texture height
 * \param DXGI_FORMAT format Texture format
 * \param D3D12_RESOURCE_STATES resourceState Initial state
 * \param ID3D12Resource** pResource Output resource pointer
 */
void D3D12VideoShaders::NISCreateTexture2D(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_STATES resourceState, ID3D12Resource** pResource)
{
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    m_hr = m_Device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, nullptr, __uuidof(ID3D12Resource), (void**)pResource);
    if(!verifyHResult(m_hr, "NISCreateTexture2D")){
        return;
    }
}

/**
 * \brief Create buffer for NIS
 *
 * Helper method to create a buffer resource with specified
 * heap type for NIS data storage.
 *
 * \param uint32_t size Buffer size in bytes
 * \param D3D12_HEAP_TYPE heapType Heap type (upload/default)
 * \param D3D12_RESOURCE_STATES resourceState Initial state
 * \param ID3D12Resource** pResource Output resource pointer
 */
void D3D12VideoShaders::NISCreateBuffer(uint32_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES resourceState, ID3D12Resource** pResource)
{
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
    auto heapProperties = CD3DX12_HEAP_PROPERTIES(heapType);
    m_hr = m_Device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, resourceState, nullptr, __uuidof(ID3D12Resource), (void**)pResource);
    if(!verifyHResult(m_hr, "NISCreateBuffer")){
        return;
    }
}

/**
 * \brief Upload buffer data for NIS
 *
 * Copies data from CPU to GPU buffer via staging resource
 * with appropriate resource state transitions.
 *
 * \param void* data Source data pointer
 * \param uint32_t size Data size in bytes
 * \param ID3D12Resource* pResource Destination buffer
 * \param ID3D12Resource* pStagingResource Staging buffer for upload
 */
void D3D12VideoShaders::NISUploadBufferData(void* data, uint32_t size, ID3D12Resource* pResource, ID3D12Resource* pStagingResource)
{
    D3D12_RESOURCE_BARRIER barrier;

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        pResource,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
        );
    m_GraphicsCommandList->ResourceBarrier(1, &barrier);

    uint8_t* mappedData = nullptr;
    pStagingResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    memcpy(mappedData, data, size);
    pStagingResource->Unmap(0, nullptr);
    m_GraphicsCommandList->CopyBufferRegion(pResource, 0, pStagingResource, 0, size);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        pResource,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON
        );
    m_GraphicsCommandList->ResourceBarrier(1, &barrier);
}

/**
 * \brief Upload texture data for NIS
 *
 * Copies texture data from CPU to GPU via staging resource
 * with proper row pitch alignment and state transitions.
 *
 * \param void* data Source data pointer
 * \param uint32_t size Data size in bytes
 * \param uint32_t rowPitch Row pitch in bytes
 * \param ID3D12Resource* pResource Destination texture
 * \param ID3D12Resource* pStagingResource Staging resource for upload
 */
void D3D12VideoShaders::NISUploadTextureData(void* data, uint32_t size, uint32_t rowPitch, ID3D12Resource* pResource, ID3D12Resource* pStagingResource)
{
    D3D12_RESOURCE_BARRIER barrier;

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        pResource,
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_COPY_DEST
        );
    m_GraphicsCommandList->ResourceBarrier(1, &barrier);

    uint8_t* mappedData = nullptr;
    pStagingResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    memcpy(mappedData, data, size);
    pStagingResource->Unmap(0, nullptr);
    D3D12_RESOURCE_DESC desc = pResource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Footprint.Width = uint32_t(desc.Width);
    footprint.Footprint.Height = uint32_t(desc.Height);
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    footprint.Footprint.Format = desc.Format;
    CD3DX12_TEXTURE_COPY_LOCATION src(pStagingResource, footprint);
    CD3DX12_TEXTURE_COPY_LOCATION dst(pResource, 0);
    m_GraphicsCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        pResource,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_COMMON
        );
    m_GraphicsCommandList->ResourceBarrier(1, &barrier);
}

/**
 * \brief Create aligned coefficient data for NIS
 *
 * Converts linear coefficient array to aligned format matching
 * D3D12 texture data pitch alignment requirements.
 *
 * \param uint16_t* data Source coefficient data
 * \param std::vector<uint16_t>& coef Output aligned coefficient vector
 * \param uint32_t rowPitchAligned Aligned row pitch in bytes
 */
void D3D12VideoShaders::NISCreateAlignedCoefficients(uint16_t* data, std::vector<uint16_t>& coef, uint32_t rowPitchAligned)
{
    const int rowElements = rowPitchAligned / sizeof(uint16_t);
    const int coefSize = rowElements * kPhaseCount;
    coef.resize(coefSize);
    for (uint32_t y = 0; y < kPhaseCount; ++y)
    {
        for (uint32_t x = 0; x < kFilterSize; ++x) {
            coef[x + y * uint64_t(rowElements)] = data[x + y * kFilterSize];
        }
    }
}
