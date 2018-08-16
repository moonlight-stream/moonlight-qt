#pragma once

#include <CoreVideo/CoreVideo.h>

#include "pacer.h"

class DisplayLinkVsyncSource : public IVsyncSource
{
public:
    DisplayLinkVsyncSource(Pacer* pacer);

    virtual ~DisplayLinkVsyncSource();

    virtual bool initialize(SDL_Window* window);

private:
    static
    CVReturn
    displayLinkOutputCallback(
        CVDisplayLinkRef,
        const CVTimeStamp* now,
        const CVTimeStamp* /* vsyncTime */,
        CVOptionFlags,
        CVOptionFlags*,
        void *displayLinkContext);

    Pacer* m_Pacer;
    CVDisplayLinkRef m_DisplayLink;
};

