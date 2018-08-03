#pragma once

#include "renderer.h"

#include <d3d9.h>
#include <dxva2api.h>

extern "C" {
#include <libavcodec/dxva2.h>
}

class DXVA2Renderer : public IFFmpegRenderer
{
public:
    DXVA2Renderer();
    virtual ~DXVA2Renderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrame(AVFrame* frame);

private:
    bool initializeDecoder();
    bool initializeRenderer();
    bool isDecoderBlacklisted();

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

    SDL_Renderer* m_SdlRenderer;

    struct dxva_context m_DXVAContext;
    IDirect3DSurface9* m_DecSurfaces[19];
    DXVA2_ConfigPictureDecode m_Config;
    IDirectXVideoDecoderService* m_DecService;
    IDirectXVideoDecoder* m_Decoder;
    int m_SurfacesUsed;
    AVBufferPool* m_Pool;

    IDirect3DDevice9* m_Device;
    IDirect3DSurface9* m_RenderTarget;
    IDirectXVideoProcessorService* m_ProcService;
    IDirectXVideoProcessor* m_Processor;
    DXVA2_ValueRange m_BrightnessRange;
    DXVA2_ValueRange m_ContrastRange;
    DXVA2_ValueRange m_HueRange;
    DXVA2_ValueRange m_SaturationRange;
    DXVA2_VideoDesc m_Desc;
    REFERENCE_TIME m_FrameIndex;
};
