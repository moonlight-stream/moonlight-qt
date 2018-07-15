#include "vt.h"

VTRenderer::VTRenderer()
{

}

VTRenderer::~VTRenderer()
{
}

bool VTRenderer::prepareDecoderContext(AVCodecContext*)
{
    /* Nothing to do */
    return true;
}

bool VTRenderer::initialize(SDL_Window* window,
                            int videoFormat,
                            int width,
                            int height)
{
    return false;
}

void VTRenderer::renderFrame(AVFrame* frame)
{
}
