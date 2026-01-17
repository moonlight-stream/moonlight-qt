#pragma once

#include <directx/d3d12.h>

#include <wrl/client.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <QString>

#include <DirectXPackedVector.h>
using namespace DirectX::PackedVector;
#include <cstdint>
#include "streaming/video/videoenhancement.h"

// NIS Declaration
#include "NIS_Config.h"

// FSR1 Declaration
#define A_CPU
#include "ffx_a.h"
#include "ffx_fsr1.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class D3D12VideoShaders
{
public:
    enum class Enhancer {
        NONE,
        CONVERT_PS,
        NIS,
        NIS_SHARPENER,
        FSR1,
        RCAS,
        COPY
    };

    static bool isUpscaler(Enhancer enhancer);
    static bool isSharpener(Enhancer enhancer);
    static bool isUsingShader(Enhancer enhancer);

    D3D12VideoShaders(
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
        );

    ~D3D12VideoShaders();

    void draw(
        D3D12_RESOURCE_STATES inputTextureStateIn,
        D3D12_RESOURCE_STATES inputTextureStateOut,
        D3D12_RESOURCE_STATES outputTextureStateIn,
        D3D12_RESOURCE_STATES outputTextureStateOut
        );

    bool updateShaderResourceView(ID3D12Resource* resource);
    void updateRootConstsOffset(XMFLOAT3 offset);

private:

    // FSR1
    struct alignas(16) FSR1Constants {
        AU1 const0[4];
        AU1 const1[4];
        AU1 const2[4];
        AU1 const3[4];
    };
    FSR1Constants m_EASUConstants;
    FSR1Constants m_RCASConstants;
    ComPtr<ID3D12PipelineState> m_PipelineStateEASU;
    ComPtr<ID3D12PipelineState> m_PipelineStateRCAS;

    struct alignas(16) RootConsts
    {
        // SDR and HDR invert bits
        // Block 0 (0->16)
        float g_INV_8BIT;
        float g_INV_10BIT;
        float _pad0[2];

        // PQ converter constants
        // Block 1 (16->32)
        float g_M1Inv;
        float g_M2Inv;
        float g_C1;
        float g_C2;
        // Block 2 (32->48)
        float g_C3;
        float _pad2[3];

        // CSC matrix rows
        // Block 3 (48->64)
        float  g_CSC_Row0_x;
        float  g_CSC_Row0_y;
        float  g_CSC_Row0_z;
        float  _pad3;

        // Block 4 (64->80)
        float  g_CSC_Row1_x;
        float  g_CSC_Row1_y;
        float  g_CSC_Row1_z;
        float  _pad4;

        // Block 5 (80->96)
        float  g_CSC_Row2_x;
        float  g_CSC_Row2_y;
        float  g_CSC_Row2_z;
        float  _pad5;

        // Color range and YUV Offset
        // Block 6 (96->112)
        float  g_ScaleY;
        float  g_OffsetY;
        float  g_OffsetU;
        float  g_OffsetV;

        // Texture format In/Out
        // Block 7 (112->128)
        uint32_t g_InputFormat;
        uint32_t g_OutputFormat;
        uint32_t g_GammaCorrection;
        uint32_t g_Range;
    };
    static_assert(sizeof(RootConsts) == 8 * 16, "RootConstsCPU must be 32 dwords (128 bytes)");

    RootConsts m_RootConsts;

    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12GraphicsCommandList> m_GraphicsCommandList;
    ComPtr<ID3D12CommandQueue> m_GraphicsCommandQueue;
    VideoEnhancement* m_VideoEnhancement;
    ComPtr<ID3D12Resource> m_TextureIn;
    ComPtr<ID3D12Resource> m_TextureOut;
    int m_InWidth;
    int m_InHeight;
    int m_OutWidth;
    int m_OutHeight;
    int m_OffsetTop;
    int m_OffsetLeft;
    Enhancer m_Enhancer = Enhancer::NONE;
    DXGI_COLOR_SPACE_TYPE m_ColorSpace;

    HRESULT m_hr;
    bool m_isYUV = false;
    bool m_IsYUV444 = false;
    bool m_IsHDR = false;
    bool m_Is2Planes = false;
    bool m_AdvancedShader = true;

    // States
    D3D12_RESOURCE_STATES m_InputTextureStateIn;
    D3D12_RESOURCE_STATES m_InputTextureStateOut;
    D3D12_RESOURCE_STATES m_OutputTextureStateIn;
    D3D12_RESOURCE_STATES m_OutputTextureStateOut;

    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeapCBV_SRV_UAV;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeapRTV;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeapSampler;
    UINT m_DescriptorSizeCBV_SRV_UAV = 0;
    UINT m_DescriptorSizeRTV = 0;
    UINT m_DescriptorSizeSampler = 0;
    UINT m_DispatchX;
    UINT m_DispatchY;
    ComPtr<ID3D12Resource> m_TextureInterm;

    bool m_IsUpscaling = true;
    bool m_IsUsingShader = true;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    // NVIDIA Image Scaling
    bool m_NISHalfprecision = true; // true: 16-bit (half precision) / false: 32-bit (full precision)
    ComPtr<ID3D12Resource> m_ConstatBuffer;
    ComPtr<ID3D12Resource> m_StagingBuffer;
    ComPtr<ID3D12Resource> m_CoefScaler;
    ComPtr<ID3D12Resource> m_CoefUSM;
    ComPtr<ID3D12Resource> m_CoefScalerUpload;
    ComPtr<ID3D12Resource> m_CoefUSMUpload;
    std::vector<uint16_t> m_CoefScalerHost;
    std::vector<uint16_t> m_CoefUSMHost;
    uint32_t m_RowPitch;
    NISConfig m_ConfigNIS;
    // Use global variable in case the initializer finish before the compilation
    std::wstring m_NIS_SCALER;
    std::wstring m_NIS_HDR_MODE;
    std::wstring m_NIS_BLOCK_WIDTH;
    std::wstring m_NIS_BLOCK_HEIGHT;
    std::wstring m_NIS_THREAD_GROUP_SIZE;
    std::wstring m_NIS_USE_HALF_PRECISION;
    std::vector<LPCWSTR> m_Args;

    bool verifyHResult(HRESULT hr, const char* operation);
    void initializeRootConsts();
    bool isSupportingAdvancedShader();

    // Helpers
    bool createDescriptorHeaps(UINT cbvSrvUavCount, UINT rtvCount, UINT samplerCount);
    bool createTexture(ComPtr<ID3D12Resource>& pTexture, int width, int height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES textureState);
    bool createCBVforResource(ID3D12Resource* resource, UINT descriptorIndex);
    bool createSRVforResource(ID3D12Resource* resource, UINT descriptorIndex, DXGI_FORMAT format, UINT planeSlice = 0);
    bool createUAVforResource(ID3D12Resource* resource, UINT descriptorIndex, DXGI_FORMAT format, UINT planeSlice = 0);
    bool createRTVforResource(ID3D12Resource* resource, UINT rtvIndex, DXGI_FORMAT format, UINT planeSlice = 0);

    bool initializeCONVERT_PS();
    bool initializeNIS(bool isUpscaling = true);
    bool initializeFSR1();
    bool initializeRCAS();
    bool initializeCOPY();

    bool applyCONVERT_PS();
    bool applyNIS();
    bool applyFSR1();
    bool applyRCAS();
    bool applyCOPY();

    // NVIDIA Image Scaling
    void NISCreateTexture2D(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_RESOURCE_STATES resourceState, ID3D12Resource** pResource);
    void NISCreateBuffer(uint32_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES resourceState, ID3D12Resource** pResource);
    void NISUploadTextureData(void* data, uint32_t size, uint32_t rowPitch, ID3D12Resource* pResource, ID3D12Resource* pStagingResource);
    void NISUploadBufferData(void* data, uint32_t size, ID3D12Resource* pResource, ID3D12Resource* pStagingResource);
    void NISCreateAlignedCoefficients(uint16_t* data, std::vector<uint16_t>& coef, uint32_t rowPitchAligned);
};
