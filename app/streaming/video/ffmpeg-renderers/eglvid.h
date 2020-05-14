#pragma once

#include "renderer.h"

class EGLRenderer : public IFFmpegRenderer {
public:
    EGLRenderer(IFFmpegRenderer *backendRenderer);
    virtual ~EGLRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS params) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual void notifyOverlayUpdated(Overlay::OverlayType) override;
    virtual bool isPixelFormatSupported(int videoFormat, enum AVPixelFormat pixelFormat) override;

private:
    using EGLImageTargetTexture2DOES_t = void (*)(int, void *);

    bool compileShader();
    bool specialize();
    const float *getColorMatrix();
    static int loadAndBuildShader(int shaderType, const char *filename);

    int m_SwPixelFormat;
    void *m_EGLDisplay;
    unsigned m_Textures[EGL_MAX_PLANES];
    unsigned m_ShaderProgram;
    SDL_GLContext m_Context;
    SDL_Window *m_Window;
    IFFmpegRenderer *m_Backend;
    unsigned int m_VAO;
    int m_ColorSpace;
    bool m_ColorFull;
    EGLImageTargetTexture2DOES_t EGLImageTargetTexture2DOES;
    SDL_Renderer *m_DummyRenderer;
};
