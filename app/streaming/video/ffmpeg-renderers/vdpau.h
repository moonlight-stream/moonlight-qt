#pragma once

#include "renderer.h"

extern "C" {
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#include <libavutil/hwcontext_vdpau.h>
}

class VDPAURenderer : public IFFmpegRenderer
{
public:
    VDPAURenderer();
    virtual ~VDPAURenderer();
    virtual bool initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height,
                            int maxFps,
                            bool enableVsync);
    virtual bool prepareDecoderContext(AVCodecContext* context);
    virtual void renderFrameAtVsync(AVFrame* frame);
    virtual bool needsTestFrame();
    virtual int getDecoderCapabilities();
    virtual FramePacingConstraint getFramePacingConstraint();

private:
    uint32_t m_VideoWidth, m_VideoHeight;
    uint32_t m_DisplayWidth, m_DisplayHeight;
    AVBufferRef* m_HwContext;
    VdpPresentationQueueTarget m_PresentationQueueTarget;
    VdpPresentationQueue m_PresentationQueue;
    VdpVideoMixer m_VideoMixer;
    VdpRGBAFormat m_OutputSurfaceFormat;
    VdpDevice m_Device;

#define OUTPUT_SURFACE_COUNT 3
    VdpOutputSurface m_OutputSurface[OUTPUT_SURFACE_COUNT];
    int m_NextSurfaceIndex;

#define OUTPUT_SURFACE_FORMAT_COUNT 2
    static const VdpRGBAFormat k_OutputFormats[OUTPUT_SURFACE_FORMAT_COUNT];

    VdpGetErrorString* m_VdpGetErrorString;
    VdpPresentationQueueTargetDestroy* m_VdpPresentationQueueTargetDestroy;
    VdpVideoMixerCreate* m_VdpVideoMixerCreate;
    VdpVideoMixerDestroy* m_VdpVideoMixerDestroy;
    VdpVideoMixerRender* m_VdpVideoMixerRender;
    VdpPresentationQueueCreate* m_VdpPresentationQueueCreate;
    VdpPresentationQueueDestroy* m_VdpPresentationQueueDestroy;
    VdpPresentationQueueDisplay* m_VdpPresentationQueueDisplay;
    VdpPresentationQueueSetBackgroundColor* m_VdpPresentationQueueSetBackgroundColor;
    VdpPresentationQueueBlockUntilSurfaceIdle* m_VdpPresentationQueueBlockUntilSurfaceIdle;
    VdpOutputSurfaceCreate* m_VdpOutputSurfaceCreate;
    VdpOutputSurfaceDestroy* m_VdpOutputSurfaceDestroy;
    VdpOutputSurfaceQueryCapabilities* m_VdpOutputSurfaceQueryCapabilities;
    VdpVideoSurfaceGetParameters* m_VdpVideoSurfaceGetParameters;
    VdpGetInformationString* m_VdpGetInformationString;

    // X11 stuff
    VdpPresentationQueueTargetCreateX11* m_VdpPresentationQueueTargetCreateX11;
};


