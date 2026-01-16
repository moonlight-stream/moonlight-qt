#pragma once

#include "renderer.h"
#include "pacer/pacer.h"

#include <d3d9.h>
#include <dxva2api.h>

#include <wrl/client.h>

class DXVA2Renderer : public IFFmpegRenderer
{
public:
    DXVA2Renderer(int decoderSelectionPass);
    virtual ~DXVA2Renderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType type) override;
    virtual int getDecoderColorspace() override;
    virtual int getDecoderCapabilities() override;

private:
    bool initializeDecoder();
    bool initializeRenderer();
    bool initializeVideoProcessor();
    bool initializeDevice(SDL_Window* window, bool enableVsync);
    bool isDecoderBlacklisted();
    bool initializeQuirksForAdapter(IDirect3D9Ex* d3d9ex, int adapterIndex);
    void renderOverlay(Overlay::OverlayType type);

    int m_DecoderSelectionPass;

    int m_VideoFormat;

    int m_DisplayWidth;
    int m_DisplayHeight;

    AVBufferRef* m_HwDeviceContext;

    SDL_SpinLock m_OverlayLock;
    std::array<Microsoft::WRL::ComPtr<IDirect3DVertexBuffer9>, Overlay::OverlayMax> m_OverlayVertexBuffers;
    std::array<Microsoft::WRL::ComPtr<IDirect3DTexture9>, Overlay::OverlayMax> m_OverlayTextures;

    Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> m_Device;
    Microsoft::WRL::ComPtr<IDirect3DDeviceManager9> m_DeviceManager;
    Microsoft::WRL::ComPtr<IDirect3DSurface9> m_RenderTarget;
    Microsoft::WRL::ComPtr<IDirectXVideoProcessorService> m_ProcService;
    Microsoft::WRL::ComPtr<IDirectXVideoProcessor> m_Processor;
    DXVA2_ValueRange m_BrightnessRange;
    DXVA2_ValueRange m_ContrastRange;
    DXVA2_ValueRange m_HueRange;
    DXVA2_ValueRange m_SaturationRange;
    DXVA2_VideoDesc m_Desc;
    REFERENCE_TIME m_FrameIndex;
    bool m_BlockingPresent;

#define DXVA2_QUIRK_NO_VP 0x01
#define DXVA2_QUIRK_SET_DEST_FORMAT 0x02
#define DXVA2_QUIRK_WDDM_20_PLUS 0x04 // Unused
#define DXVA2_QUIRK_MULTI_GPU 0x08
    int m_DeviceQuirks;
};
