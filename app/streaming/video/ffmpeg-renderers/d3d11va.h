#pragma once

#include "renderer.h"

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <CGuid.h>
#include <atlbase.h>
#include "streaming/video/videoenhancement.h"
#include "public/common/AMFFactory.h"

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

#include <wrl/client.h>

class D3D11VARenderer : public IFFmpegRenderer
{
public:
    D3D11VARenderer(int decoderSelectionPass);
    virtual ~D3D11VARenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary**) override;
    virtual bool prepareDecoderContextInGetFormat(AVCodecContext* context, AVPixelFormat pixelFormat) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual int getRendererAttributes() override;
    virtual int getDecoderCapabilities() override;
    virtual bool needsTestFrame() override;
    virtual void setHdrMode(bool enabled) override;
    virtual InitFailureReason getInitFailureReason() override;

    enum PixelShaders {
        GENERIC_YUV_420,
        BT_601_LIMITED_YUV_420,
        BT_2020_LIMITED_YUV_420,
        GENERIC_AYUV,
        GENERIC_Y410,
        _COUNT
    };

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool setupRenderingResources();
    bool setupAmfTexture();
    std::vector<DXGI_FORMAT> getVideoTextureSRVFormats();
    bool setupVideoTexture(); // for !m_BindDecoderOutputTextures
    bool setupTexturePoolViews(AVD3D11VAFramesContext* frameContext); // for m_BindDecoderOutputTextures
    bool setupEnhancedTexture();
    void renderOverlay(Overlay::OverlayType type);
    void bindColorConversion(AVFrame* frame);
    void prepareEnhancedOutput(AVFrame* frame);
    void renderVideo(AVFrame* frame);
    bool createVideoProcessor();
    bool initializeVideoProcessor();
    bool enableAMDVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableIntelVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableNvidiaVideoSuperResolution(bool activate = true, bool logInfo = true);
    bool enableAMDHDR(bool activate = true, bool logInfo = true);
    bool enableIntelHDR(bool activate = true, bool logInfo = true);
    bool enableNvidiaHDR(bool activate = true, bool logInfo = true);
    bool checkDecoderSupport(IDXGIAdapter* adapter);
    int getAdapterIndexByEnhancementCapabilities();
    bool createDeviceByAdapterIndex(int adapterIndex, bool* adapterNotFound = nullptr);

    int m_DecoderSelectionPass;
    int m_DevicesWithFL11Support;
    int m_DevicesWithCodecSupport;

    int m_AdapterIndex = 0;
    int m_OutputIndex = 0;

    enum class SupportedFenceType {
        None,
        NonMonitored,
        Monitored,
    };

    Microsoft::WRL::ComPtr<IDXGIFactory5> m_Factory;
    Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_SwapChain;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_DeviceContext;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
    SupportedFenceType m_FenceType;
    SDL_mutex* m_ContextLock;
    bool m_BindDecoderOutputTextures;
    D3D11_BOX m_SrcBox;

    Microsoft::WRL::ComPtr<ID3D11VideoDevice> m_VideoDevice;
    Microsoft::WRL::ComPtr<ID3D11VideoContext2> m_VideoContext;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> m_VideoProcessor;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnumerator;
    D3D11_VIDEO_PROCESSOR_CAPS m_VideoProcessorCapabilities;
    D3D11_VIDEO_PROCESSOR_STREAM m_StreamData;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> m_OutputView;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> m_InputView;
    Microsoft::WRL::ComPtr<ID3D11Resource> m_BackBufferResource;
    VideoEnhancement* m_VideoEnhancement;
    bool m_AutoStreamSuperResolution = false;
    bool m_UseFenceHack;

    DECODER_PARAMETERS m_DecoderParams;
    int m_TextureAlignment;
    DXGI_FORMAT m_TextureFormat;
    int m_DisplayWidth;
    int m_DisplayHeight;
    int m_LastColorSpace;
    bool m_LastFullRange;
    AVColorTransferCharacteristic m_LastColorTrc;

    bool m_AllowTearing;

    std::array<Microsoft::WRL::ComPtr<ID3D11PixelShader>, PixelShaders::_COUNT> m_VideoPixelShaders;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_VideoVertexBuffer;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_AmfTexture;
    // Only valid if !m_BindDecoderOutputTextures
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_VideoTexture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_EnhancedTexture;

    // Only index 0 is valid if !m_BindDecoderOutputTextures
#define DECODER_BUFFER_POOL_SIZE 17
    std::array<std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, 2>, DECODER_BUFFER_POOL_SIZE> m_VideoTextureResourceViews;

    struct {
        int width;
        int height;
        int left;
        int top;
    } m_OutputTexture;

    SDL_SpinLock m_OverlayLock;
    std::array<Microsoft::WRL::ComPtr<ID3D11Buffer>, Overlay::OverlayMax> m_OverlayVertexBuffers;
    std::array<Microsoft::WRL::ComPtr<ID3D11Texture2D>, Overlay::OverlayMax> m_OverlayTextures;
    std::array<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, Overlay::OverlayMax> m_OverlayTextureResourceViews;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_OverlayPixelShader;

    AVBufferRef* m_HwDeviceContext;
    AVBufferRef* m_HwFramesContext;

    // AMD (AMF)
    amf::AMFContextPtr m_AmfContext;
    amf::AMFSurfacePtr m_AmfSurface;
    amf::AMFDataPtr m_AmfData;
    // amf::AMFComponentPtr does not work for m_AmfUpScaler, have to use raw pointer
    amf::AMFComponent* m_AmfUpScaler;
    bool m_AmfInitialized = false;
    bool m_AmfUpScalerSharpness = false;
    amf::AMF_SURFACE_FORMAT m_AmfUpScalerSurfaceFormat;

};

