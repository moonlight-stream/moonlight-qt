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
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual int getRendererAttributes() override;
    virtual int getDecoderCapabilities() override;
    virtual bool needsTestFrame() override;
    virtual void setHdrMode(bool enabled) override;

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool setupRenderingResources();
    bool setupVideoTexture();
    bool setupFrameTexture();
    void renderOverlay(Overlay::OverlayType type);
    void bindColorConversion(AVFrame* frame);
    void prepareVideoProcessorStream(AVFrame* frame);
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

    int m_AdapterIndex = 0;
    int m_OutputIndex = 0;
    ComPtr<IDXGIFactory5> m_Factory;
    // Cannt convert to ComPtr because of av_buffer_unref()
    ID3D11Device* m_Device;
    ID3D11DeviceContext* m_DeviceContext;
    ComPtr<IDXGISwapChain4> m_SwapChain;
    ID3D11RenderTargetView* m_RenderTargetView;
    SDL_mutex* m_ContextLock;

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

    // Variable unused, but keep it as reference for debugging purpose
    DXGI_COLOR_SPACE_TYPE m_ColorSpaces[26] = {
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,           // 0  -       A
        DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,           // 1  -       A
        DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709,         // 2  - I   * A
        DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020,        // 3  -    I*
        DXGI_COLOR_SPACE_RESERVED,                         // 4
        DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601,    // 5  -  O    A
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601,       // 6  - I     A
        DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601,         // 7  -  O    A
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,       // 8  - I     A
        DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,         // 9  -       A
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020,      // 10 -    I
        DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020,        // 11 -  O
        DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020,        // 12 -  O  O
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020,    // 13 -    I
        DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020,      // 14 - I  I*
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020,   // 15 -    I
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020, // 16 -    I
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020,          // 17 - I  I*
        DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020,  // 18 -    I
        DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020,    // 19 -    I
        DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P709,         // 20 - I  I*
        DXGI_COLOR_SPACE_RGB_STUDIO_G24_NONE_P2020,        // 21 -    I*
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P709,       // 22 -    I
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_LEFT_P2020,      // 23 - I  I
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G24_TOPLEFT_P2020,   // 24 -    I
        DXGI_COLOR_SPACE_CUSTOM,                           // 25
    };

    DECODER_PARAMETERS m_DecoderParams;
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

    ComPtr<ID3D11Texture2D> m_FrameTexture;
    ComPtr<ID3D11Texture2D> m_VideoTexture;
    ID3D11ShaderResourceView* m_VideoTextureResourceViews[2];
    float m_ScaleUp = 1;
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

    // AMD (AMF)
    amf::AMFContextPtr m_AmfContext;
    amf::AMFSurfacePtr m_AmfInputSurface;
    amf::AMFComponentPtr m_AmfDenoiser;
    amf::AMFComponentPtr m_AmfFormatConverter;
    amf::AMFComponentPtr m_AmfUpScaler;
    // amf::AMFComponentPtr does not work for m_AmfDownScaler, have to use raw pointer
    amf::AMFComponent* m_AmfDownScaler;

    bool m_AmfInitialized = false;

};

