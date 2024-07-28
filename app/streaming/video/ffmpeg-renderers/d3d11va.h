#pragma once

#include "renderer.h"

#include <d3d11_4.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <CGuid.h>
#include <atlbase.h>
#include "streaming/video/videoenhancement.h"
#include "public/common/AMFFactory.h"

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

using Microsoft::WRL::ComPtr;

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

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool setupRenderingResources();
    bool setupAmfTexture();
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
    int m_DevicesWithCodecSupport;
    int m_DevicesWithFL11Support;

    int m_AdapterIndex = 0;
    int m_OutputIndex = 0;
    ComPtr<IDXGIFactory5> m_Factory;
    // Cannt convert to ComPtr because of av_buffer_unref()
    ID3D11Device* m_Device;
    ID3D11DeviceContext* m_DeviceContext;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    ID3D11RenderTargetView* m_RenderTargetView;
    SDL_mutex* m_ContextLock;
    bool m_BindDecoderOutputTextures;
    D3D11_BOX m_SrcBox;

    ComPtr<ID3D11VideoDevice> m_VideoDevice;
    ComPtr<ID3D11VideoContext2> m_VideoContext;
    ComPtr<ID3D11VideoProcessor> m_VideoProcessor;
    ComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnumerator;
    D3D11_VIDEO_PROCESSOR_CAPS m_VideoProcessorCapabilities;
    D3D11_VIDEO_PROCESSOR_STREAM m_StreamData;
    ComPtr<ID3D11VideoProcessorOutputView> m_OutputView;
    ComPtr<ID3D11VideoProcessorInputView> m_InputView;
    ComPtr<ID3D11Resource> m_BackBufferResource;
    VideoEnhancement* m_VideoEnhancement;
    bool m_AutoStreamSuperResolution = false;

    DECODER_PARAMETERS m_DecoderParams;
    int m_TextureAlignment;
    int m_DisplayWidth;
    int m_DisplayHeight;
    int m_LastColorSpace;
    bool m_LastFullRange;
    AVColorTransferCharacteristic m_LastColorTrc;

    bool m_AllowTearing;

    ComPtr<ID3D11PixelShader> m_VideoGenericPixelShader;
    ComPtr<ID3D11PixelShader> m_VideoBt601LimPixelShader;
    ComPtr<ID3D11PixelShader> m_VideoBt2020LimPixelShader;
    ComPtr<ID3D11Buffer> m_VideoVertexBuffer;

    ComPtr<ID3D11Texture2D> m_AmfTexture;
    // Only valid if !m_BindDecoderOutputTextures
    ComPtr<ID3D11Texture2D> m_VideoTexture;
    ComPtr<ID3D11Texture2D> m_EnhancedTexture;

    // Only index 0 is valid if !m_BindDecoderOutputTextures
#define DECODER_BUFFER_POOL_SIZE 17
    ID3D11ShaderResourceView* m_VideoTextureResourceViews[DECODER_BUFFER_POOL_SIZE][2];

    struct {
        int width;
        int height;
        int left;
        int top;
    } m_OutputTexture;

    SDL_SpinLock m_OverlayLock;
    ComPtr<ID3D11Buffer> m_OverlayVertexBuffers[Overlay::OverlayMax];
    ComPtr<ID3D11Texture2D> m_OverlayTextures[Overlay::OverlayMax];
    ComPtr<ID3D11ShaderResourceView> m_OverlayTextureResourceViews[Overlay::OverlayMax];
    ComPtr<ID3D11PixelShader> m_OverlayPixelShader;

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
