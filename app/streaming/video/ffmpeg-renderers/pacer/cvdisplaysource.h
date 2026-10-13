#pragma once

#include "pacer.h"

// CoreVideo is pure C, so it's safe here. AppKit isn't, so it's in the .mm.
#include <CoreVideo/CoreVideo.h>

// Waits for the next vertical blank using a CVDisplayLink.
class CVDisplaySource : public IVsyncSource {
public:
    explicit CVDisplaySource(Pacer* pacer);

    virtual ~CVDisplaySource();

    bool initialize(SDL_Window* window, int displayFps) override;

    // The CVDisplayLink callback calls Pacer::signalVsync() for us.
    bool isAsync() override { return true; }

private:
    // NB: Must match CVDisplayLinkOutputCallback exactly. A mismatched
    // signature passed by cast corrupts the arguments signalVsync() sees.
    static CVReturn displayLinkCallback(CVDisplayLinkRef displayLink,
                                        const CVTimeStamp* now,
                                        const CVTimeStamp* vsyncTime,
                                        CVOptionFlags flags,
                                        CVOptionFlags* newFlags,
                                        void* context);

    Pacer* m_Pacer;
    CVDisplayLinkRef m_DisplayLink;
};