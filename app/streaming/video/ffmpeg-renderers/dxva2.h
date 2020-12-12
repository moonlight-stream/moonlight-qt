#pragma once

#include "renderer.h"
#include "pacer/pacer.h"

#include <d3d9.h>
#include <dxva2api.h>

#ifdef HAS_D3DX9
#include <d3dx9.h>
#endif

extern "C" {
#include <libavcodec/dxva2.h>
}

class DXVA2Renderer : public IFFmpegRenderer
{
public:
    DXVA2Renderer();
    virtual ~DXVA2Renderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual int getDecoderColorspace() override;

private:
    bool initializeDecoder();
    bool initializeRenderer();
    bool initializeDevice(SDL_Window* window, bool enableVsync);
    bool isDecoderBlacklisted();
    bool isDXVideoProcessorAPIBlacklisted();

    static
    AVBufferRef* ffPoolAlloc(void* opaque, int size);

    static
    void ffPoolDummyDelete(void*, uint8_t*);

    static
    int ffGetBuffer2(AVCodecContext* context, AVFrame* frame, int flags);

    int m_VideoFormat;
    int m_VideoWidth;
    int m_VideoHeight;

    int m_DisplayWidth;
    int m_DisplayHeight;

    struct dxva_context m_DXVAContext;
    IDirect3DSurface9* m_DecSurfaces[19];
    DXVA2_ConfigPictureDecode m_Config;
    IDirectXVideoDecoderService* m_DecService;
    IDirectXVideoDecoder* m_Decoder;
    int m_SurfacesUsed;
    AVBufferPool* m_Pool;

    IDirect3DDevice9Ex* m_Device;
    IDirect3DSurface9* m_RenderTarget;
    IDirectXVideoProcessorService* m_ProcService;
    IDirectXVideoProcessor* m_Processor;
    DXVA2_ValueRange m_BrightnessRange;
    DXVA2_ValueRange m_ContrastRange;
    DXVA2_ValueRange m_HueRange;
    DXVA2_ValueRange m_SaturationRange;
    DXVA2_VideoDesc m_Desc;
    REFERENCE_TIME m_FrameIndex;
#ifdef HAS_D3DX9
    LPD3DXFONT m_DebugOverlayFont;
    LPD3DXFONT m_StatusOverlayFont;
#endif
    bool m_BlockingPresent;
};
