#pragma once

#include <dxgi1_6.h>
#include <directx/d3d12.h>
#include <directx/d3d12video.h>
#include <d3d11_4.h>

#include "streaming/video/videoenhancement.h"
#include "settings/streamingpreferences.h"
#include "streaming/video/decoder.h"
#include "d3d12va_shaders.h"
#include "renderer.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QElapsedTimer>

// AMD AMF
#include "public/common/AMFFactory.h"
#include "public/include/core/Context.h"
#include "public/include/core/Factory.h"
#include "public/include/core/Result.h"
using namespace amf;

// NVIDIA VSR
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs.h>
#include <nvsdk_ngx_defs_truehdr.h>
#include <nvsdk_ngx_helpers_truehdr.h>
#include <nvsdk_ngx_defs_vsr.h>
#include <nvsdk_ngx_helpers_vsr.h>

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/hwcontext_d3d12va.h>
}

#define DECODER_BUFFER_POOL_SIZE 17

// NVIDIA VSR
#define APP_ID      0
#define APP_PATH    L"."

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#define ALIGN16(value) (((value + 15) >> 4) << 4)

struct decoder {
    int ColorRange;
    DXGI_FORMAT Format;
    AVPixelFormat AVFormat;
    DXGI_COLOR_SPACE_TYPE ColorSpace;
};

class D3D12VARenderer : public QObject, public IFFmpegRenderer
{
public:
    D3D12VARenderer(int decoderSelectionPass);
    virtual ~D3D12VARenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual void setHdrMode(bool enabled) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary**) override;
    virtual bool prepareDecoderContextInGetFormat(AVCodecContext* context, AVPixelFormat pixelFormat) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual bool notifyWindowChanged(PWINDOW_STATE_CHANGE_INFO info) override;
    virtual int getRendererAttributes() override;
    virtual int getDecoderCapabilities() override;
    virtual int getDecoderColorspace() override;
    virtual int getDecoderColorRange() override;
    virtual bool needsTestFrame() override;

    struct TextureInfo {
        int width;
        int height;
        int left;
        int top;
    };

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool verifyHResult(HRESULT hr, const char* operation);
    bool checkDecoderType();
    bool setupResources();
    void enhanceAutoSelection();
    bool enableAMDVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableIntelVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableNvidiaVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableAMDHDR(bool activate = true, bool logInfo = true);
    bool enableIntelHDR(bool activate = true, bool logInfo = true);
    bool enableNvidiaHDR(bool activate = true, bool logInfo = true);
    void setAMDHdr();
    bool isShader62supported();
    bool isNvidiaVSRSupport();
    bool isIntelFSR1Support();
    bool getDisplayHDRStatus();
    void updateDisplayHDRStatusAsync();
    void waitForDecoder();
    void waitForVideoProcess();
    void waitForGraphics();
    void waitForOverlay();
    void renderOverlay(Overlay::OverlayType type);

    bool initialiazeAdapterInformation();
    void TimerInfo(const char* comment, bool start);

    // Inform which step of the renderering process need to follow fr YUV->RGB concersion, then for Upscaling
    enum class RenderStep {
        ALL_VIDEOPROCESSOR,
        ALL_AMF,
        CONVERT_SHADER,
        CONVERT_VIDEOPROCESSOR,
        CONVERT_AMF,
        UPSCALE_SHADER,
        UPSCALE_VIDEOPROCESSOR,
        UPSCALE_AMF,
        UPSCALE_VSR,
        SHARPEN_SHADER,
        NONE
    };
    RenderStep m_RenderStep1 = RenderStep::CONVERT_SHADER;
    RenderStep m_RenderStep2 = RenderStep::NONE;

    struct VERTEX {
        float x, y; // Position
        float u, v; // Texcoord
        float r, g, b, a; // Background color
    };
    UINT m_VbSize;
    D3D12_VERTEX_BUFFER_VIEW m_VbView;


    // ---- DEBUG ----
    // Change to true to get debugging more verbose
    // Note: At true, it bugs VideoProcessCommandList->close()
    bool m_DebugVerbose = false;
    // Change to true to see rendering steps
    // Note: At true, it could freeze your IDE due to large amount of logs to display
    bool m_TimerInfo = false;


    bool m_IsDecoderHDR = false;
    bool m_yuv444 = false;
    DECODER_PARAMETERS m_DecoderParams;
    DXGI_ADAPTER_DESC1 m_AdapterDesc;
    UINT m_OutputIndex = 0;
    UINT m_AdapterIndex = 0;
    ComPtr<IDXGIFactory6> m_Factory;
    ComPtr<IDXGIAdapter1> m_Adapter;
    ComPtr<ID3D12Device9> m_Device;
    ComPtr<ID3D12VideoDevice2> m_VideoDevice;
    decoder m_Decoder;
    bool m_SkipRenderStep2 = false;
    int m_LastColorSpace = -1;
    bool m_LastFullRange;
    UINT m_MaxLuminance = 1000;

    HRESULT m_hr;
    VideoEnhancement* m_VideoEnhancement;
    StreamingPreferences* m_Preferences;
    int m_DisplayWidth;
    int m_DisplayHeight;
    TextureInfo m_OutputTextureInfo;
    ComPtr<ID3D12Resource> m_FrameTexture;
    ComPtr<ID3D12Resource> m_RGBTexture;
    ComPtr<ID3D12Resource> m_RGBTextureUpscaled;
    ComPtr<ID3D12Resource> m_YUVTextureUpscaled;
    ComPtr<ID3D12Resource> m_OutputTexture;
    DXGI_FORMAT m_RGBFormat;
    DXGI_COLOR_SPACE_TYPE m_RGBColorSpace;
    bool m_AllowTearing;
    // Help to redraw if the user switch to HDR rendering on its display
    bool m_IsDisplayHDRenabled = false;
    bool m_cancelHDRUpdate = false;
    QFuture<void> m_HDRUpdateFuture = QFuture<void>(); // Explicit initialization
    bool m_PauseHDRUpdate = false;
    int m_CheckHDRCount = 0;
    DXGI_HDR_METADATA_HDR10 m_StreamHDRMetaData;
    DXGI_HDR_METADATA_HDR10 m_OutputHDRMetaData;

    SDL_SpinLock m_OverlayLock;
    std::array<ComPtr<ID3D12Resource>, Overlay::OverlayMax> m_OverlayVertexBuffers;
    std::array<ComPtr<ID3D12Resource>, Overlay::OverlayMax> m_OverlayTextures;
    ComPtr<ID3D12RootSignature> m_OverlayRootSignature;
    ComPtr<ID3D12PipelineState> m_OverlayPSO;
    ComPtr<ID3D12DescriptorHeap> m_OverlaySrvHeap;

    QElapsedTimer m_Timer;
    QElapsedTimer m_TimerFPS;
    uint m_Seconds = 0;

    const UINT m_FrameCount = 3;
    UINT m_CurrentFrameIndex  = 0;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    UINT m_RtvDescriptorSize = 0;
    std::vector<ComPtr<ID3D12Resource>> m_BackBuffers;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_BackBufferRTVs;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    HANDLE m_FrameLatencyWaitableObject;

    bool m_VideoProcessorConvertEnabled = false;
    bool m_VideoProcessorUpscalerEnabled = false;
    bool m_VideoProcessorUpscalerConvertEnabled = false;
    INT32 m_NoiseReductionValue = 0;
    INT32 m_EdgeEnhancementValue = 0;
    bool m_VideoProcessorAutoProcessing = false;
    ComPtr<ID3D12VideoProcessor1> m_VideoProcessorConvert;
    ComPtr<ID3D12VideoProcessor1> m_VideoProcessorUpscaler;
    ComPtr<ID3D12VideoProcessor1> m_VideoProcessorUpscalerConvert;
    std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1> m_InputArgsConvert;
    std::vector<D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS> m_OutputArgsConvert;
    std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1> m_InputArgsUpscaler;
    std::vector<D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS> m_OutputArgsUpscaler;
    std::vector<D3D12_VIDEO_PROCESS_INPUT_STREAM_ARGUMENTS1> m_InputArgsUpscalerConvert;
    std::vector<D3D12_VIDEO_PROCESS_OUTPUT_STREAM_ARGUMENTS> m_OutputArgsUpscalerConvert;

    ComPtr<ID3D12CommandAllocator> m_DecoderCommandAllocator;
    ComPtr<ID3D12CommandList> m_DecoderCommandList;
    ComPtr<ID3D12CommandQueue> m_DecoderCommandQueue;
    ComPtr<ID3D12CommandAllocator> m_VideoProcessCommandAllocator;
    ComPtr<ID3D12VideoProcessCommandList3> m_VideoProcessCommandList;
    ComPtr<ID3D12CommandQueue> m_VideoProcessCommandQueue;
    ComPtr<ID3D12CommandAllocator> m_GraphicsCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> m_GraphicsCommandList;
    ComPtr<ID3D12CommandQueue> m_GraphicsCommandQueue;
    ComPtr<ID3D12CommandAllocator> m_OverlayCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> m_OverlayCommandList;
    ComPtr<ID3D12CommandQueue> m_OverlayCommandQueue;
    D3D12_RESOURCE_BARRIER m_Barrier;
    D3D12_BOX m_OutputBox;

    // Wait for next frame to save compute
    ComPtr<ID3D12Fence> m_FenceDecoder;
    UINT64 m_FenceDecoderValue = 0;
    HANDLE m_FenceDecoderEvent = nullptr;
    ComPtr<ID3D12Fence> m_FenceVideoProcess;
    UINT64 m_FenceVideoProcessValue = 0;
    HANDLE m_FenceVideoProcessEvent = nullptr;
    ComPtr<ID3D12Fence> m_FenceGraphics;
    UINT64 m_FenceGraphicsValue = 0;
    HANDLE m_FenceGraphicsEvent = nullptr;
    ComPtr<ID3D12Fence> m_FenceOverlay;
    UINT64 m_FenceOverlayValue = 0;
    HANDLE m_FenceOverlayEvent = nullptr;
    ComPtr<ID3D12Fence> m_FenceAMF;
    UINT64 m_FenceAMFValue = 0;
    HANDLE m_FenceAMFEvent = nullptr;

    AVBufferRef* m_HwDeviceContext;
    AVBufferRef* m_HwFramesContext;
    int m_TextureAlignment;
    int m_FrameWidth;
    int m_FrameHeight;
    AVD3D12VAFramesContext* m_D3D12FramesContext;

    // D3D11 to D3D12 transfer
    ComPtr<ID3D11Device5>     m_D3D11Device;
    ComPtr<ID3D11DeviceContext4> m_D3D11DeviceContext;
    AVD3D11VAFramesContext* m_D3D11FramesContext;
    ComPtr<ID3D11Texture2D> m_D3D11FrameTexture;
    ComPtr<ID3D11Texture2D> m_D3D11YUVTextureUpscaled;
    ComPtr<ID3D11Fence> m_D3D11Fence;
    ComPtr<ID3D12Fence> m_D3D12Fence;
    UINT64 m_D3D11FenceValue = 1;
    D3D11_BOX m_D3D11SrcBox;
    D3D12_BOX m_SrcBox;

    ComPtr<ID3D12QueryHeap> m_QueryHeap;
    int m_DecoderSelectionPass;
    UINT m_BackBufferIndex = 0;

    bool m_IsIntegratedGPU = false;
    bool m_IsOnBattery = false;
    bool m_IsLowEndGPU = false;
    bool m_VendorVSRenabled = false;
    bool m_VendorHDRenabled = false;
    bool m_AmfInitialized = false;
    bool m_IntelInitialized = false;
    bool m_NvidiaInitialized = false;

    DXGI_FORMAT m_TextureFormat;

    // Shaders class
    D3D12VideoShaders::Enhancer m_EnhancerType = D3D12VideoShaders::Enhancer::NONE;
    std::unique_ptr<D3D12VideoShaders> m_ShaderConverter = nullptr;
    std::unique_ptr<D3D12VideoShaders> m_ShaderUpscaler = nullptr;
    std::unique_ptr<D3D12VideoShaders> m_ShaderSharpener = nullptr;

    // AMD AMF
    AMFContext2Ptr m_AmfContext;
    AMFComputePtr m_AmfCompute;
    ComPtr<ID3D12CommandQueue> m_AmfCommandQueue;
    AMFDataPtr m_AmfData;
    bool m_AmfUpscalerSharpness = false;
    ComPtr<ID3D12Fence> m_AmfFence;
    UINT64 m_AmfFenceValue = 1;
    AMFBufferPtr m_HdrBuffer;
    // Correct result in HDR with m_AmfHdrEnabled at false for unknown reasons
    bool m_AmfHdrColorSpaceEnabled = false;
    // AMFComponentPtr does not work for m_AmfUpscaler, have to use raw pointer
    AMFComponent* m_AmfUpscalerYUV;
    AMFComponentPtr m_AmfVideoConverter;
    AMFComponent* m_AmfUpscalerRGB;
    AMFComponentPtr m_AmfVideoConverterUpscaled;
    AMFSurfacePtr m_AmfSurfaceYUV;
    AMFSurfacePtr m_AmfSurfaceRGB;
    AMFSurfacePtr m_AmfSurfaceUpscaledYUV;
    AMFSurfacePtr m_AmfSurfaceUpscaledRGB;
    
    // NVIDIA VSR
    bool m_bNGXInitialized = false;
    NVSDK_NGX_Parameter* m_VSRngxParameters = nullptr;
    NVSDK_NGX_Parameter* m_TrueHDRngxParameters = nullptr;
    NVSDK_NGX_Handle* m_VSRFeature = nullptr;
    NVSDK_NGX_Handle* m_TrueHDRFeature = nullptr;
    RECT m_NGXSrcRect = {};
    RECT m_NGXDstRect = {};
    
    // Used for debug purpose only
    ComPtr<ID3D12CommandAllocator> m_PictureCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> m_PictureCommandList;
    ComPtr<ID3D12CommandQueue> m_PictureCommandQueue;
    void DebugExportToPNG(
        ID3D12Resource* srctexture,
        D3D12_RESOURCE_STATES state,
        const char* filename);
};
