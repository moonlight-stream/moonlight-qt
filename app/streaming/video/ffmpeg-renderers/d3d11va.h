#pragma once

#include "renderer.h"

#include <d3d11_4.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <CGuid.h>
#include <atlbase.h>
#include "streaming/video/videoenhancement.h"

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

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

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool setupRenderingResources();
    bool setupVideoTexture();
    void renderOverlay(Overlay::OverlayType type);
    void bindColorConversion(AVFrame* frame);
    void prepareVideoProcessorStream(AVFrame* frame);
    void renderVideo(AVFrame* frame);
    bool createVideoProcessor(bool reset = false);
    void enableAMDVideoSuperResolution(bool activate = true);
    void enableIntelVideoSuperResolution(bool activate = true);
    void enableNvidiaVideoSuperResolution(bool activate = true);
    void enableAMDHDR(bool activate = true);
    void enableIntelHDR(bool activate = true);
    void enableNvidiaHDR(bool activate = true);
    bool checkDecoderSupport(IDXGIAdapter* adapter);
    bool createDeviceByAdapterIndex(int adapterIndex, bool* adapterNotFound = nullptr);
    void setHDRStream();
    void setHDROutPut();

    int m_DecoderSelectionPass;

    int m_AdapterIndex = 0;
    int m_OutputIndex = 0;
    IDXGIFactory5* m_Factory;
    ID3D11Device* m_Device;
    IDXGISwapChain4* m_SwapChain;
    ID3D11DeviceContext* m_DeviceContext;
    ID3D11RenderTargetView* m_RenderTargetView;
    SDL_mutex* m_ContextLock;

    ID3D11VideoDevice* m_VideoDevice;
    ID3D11VideoContext2* m_VideoContext;
    // CComPtr<ID3D11VideoProcessor> m_VideoProcessor;
    // CComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnumerator;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> m_VideoProcessor;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> m_VideoProcessorEnumerator;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC m_OutputViewDesc;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC m_InputViewDesc;
    D3D11_VIDEO_PROCESSOR_STREAM m_StreamData;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> m_OutputView;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> m_InputView;
    ID3D11Resource* m_BackBufferResource;
    VideoEnhancement* m_VideoEnhancement;

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

    DXGI_COLOR_SPACE_TYPE m_StreamColorSpace = m_ColorSpaces[8]; // SDR-Only (HDR is 14)
    DXGI_COLOR_SPACE_TYPE m_OutputColorSpace = m_ColorSpaces[12]; // SDR & HDR

    // [TODO] Remove the timer feature once the bug with VideoProcessorSetStreamColorSpace1 is fixed
    bool m_SetStreamColorSpace = true;
    long m_StartTime;
    long m_NextTime;
    int m_StreamIndex = 0;
    int m_Increment = 300;
    DXGI_COLOR_SPACE_TYPE m_StreamColorSpacesFixHDR[2] = {
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020,    // 13
        DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020,      // 14
    };
    DXGI_COLOR_SPACE_TYPE m_StreamColorSpacesFixSDR[2] = {
        DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709,         // 9
        DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,       // 8
    };
    ID3D11ShaderResourceView* m_VideoTextureResourceView;

    DECODER_PARAMETERS m_DecoderParams;
    int m_DisplayWidth;
    int m_DisplayHeight;
    int m_LastColorSpace;
    bool m_LastFullRange;
    AVColorTransferCharacteristic m_LastColorTrc;

    bool m_AllowTearing;

    ID3D11PixelShader* m_VideoGenericPixelShader;
    ID3D11PixelShader* m_VideoBt601LimPixelShader;
    ID3D11PixelShader* m_VideoBt2020LimPixelShader;
    ID3D11Buffer* m_VideoVertexBuffer;

    ID3D11Texture2D* m_VideoTexture;
    ID3D11ShaderResourceView* m_VideoTextureResourceViews[2];

    SDL_SpinLock m_OverlayLock;
    ID3D11Buffer* m_OverlayVertexBuffers[Overlay::OverlayMax];
    ID3D11Texture2D* m_OverlayTextures[Overlay::OverlayMax];
    ID3D11ShaderResourceView* m_OverlayTextureResourceViews[Overlay::OverlayMax];
    ID3D11PixelShader* m_OverlayPixelShader;

    AVBufferRef* m_HwDeviceContext;
};

