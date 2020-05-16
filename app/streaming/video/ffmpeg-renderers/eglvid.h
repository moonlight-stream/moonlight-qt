#pragma once

#include "renderer.h"

#include <SDL_opengles2.h>
#include <SDL_opengles2_gl2ext.h>

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

    bool compileShader();
    bool specialize();
    const float *getColorMatrix();
    static int loadAndBuildShader(int shaderType, const char *filename);
    bool openDisplay(unsigned int platform, void* nativeDisplay);

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
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC m_glEGLImageTargetTexture2DOES;
    PFNGLGENVERTEXARRAYSOESPROC m_glGenVertexArraysOES;
    PFNGLBINDVERTEXARRAYOESPROC m_glBindVertexArrayOES;
    PFNGLDELETEVERTEXARRAYSOESPROC m_glDeleteVertexArraysOES;

    int m_OldContextProfileMask;
    int m_OldContextMajorVersion;
    int m_OldContextMinorVersion;

    SDL_Renderer *m_DummyRenderer;
};
