#pragma once

#include "renderer.h"
#include "pacer/pacer.h"

#include <d3d11_1.h>
#include <dxgi1_5.h>

extern "C" {
#include <libavutil/hwcontext_d3d11va.h>
}

class D3D11VARenderer : public IFFmpegRenderer
{
public:
    D3D11VARenderer();
    virtual ~D3D11VARenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary**) override;
    virtual bool prepareDecoderContextInGetFormat(AVCodecContext* context, AVPixelFormat pixelFormat) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual void setHdrMode(bool enabled) override;
    virtual int getRendererAttributes() override;

private:
    static void lockContext(void* lock_ctx);
    static void unlockContext(void* lock_ctx);

    bool setupRenderingResources();
    void renderOverlay(Overlay::OverlayType type);
    void updateColorConversionConstants(AVFrame* frame);
    void renderVideo(AVFrame* frame);
    bool checkDecoderSupport(IDXGIAdapter* adapter);

    IDXGIFactory5* m_Factory;
    ID3D11Device* m_Device;
    IDXGISwapChain4* m_SwapChain;
    ID3D11DeviceContext* m_DeviceContext;
    ID3D11RenderTargetView* m_RenderTargetView;
    SDL_mutex* m_ContextLock;

    DECODER_PARAMETERS m_DecoderParams;
    int m_DisplayWidth;
    int m_DisplayHeight;
    bool m_Windowed;
    AVColorSpace m_LastColorSpace;
    AVColorRange m_LastColorRange;

    bool m_AllowTearing;
    HANDLE m_FrameWaitableObject;

    ID3D11PixelShader* m_VideoPixelShader;
    ID3D11Buffer* m_VideoVertexBuffer;

    SDL_SpinLock m_OverlayLock;
    ID3D11Buffer* m_OverlayVertexBuffers[Overlay::OverlayMax];
    ID3D11Texture2D* m_OverlayTextures[Overlay::OverlayMax];
    ID3D11ShaderResourceView* m_OverlayTextureResourceViews[Overlay::OverlayMax];
    ID3D11PixelShader* m_OverlayPixelShader;

    AVBufferRef* m_HwDeviceContext;
    AVBufferRef* m_HwFramesContext;
};

