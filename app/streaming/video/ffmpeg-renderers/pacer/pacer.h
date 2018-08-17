#pragma once

#include "../renderer.h"

#include <QQueue>

class IVsyncSource {
public:
    virtual ~IVsyncSource() {}
    virtual bool initialize(SDL_Window* window) = 0;
};

class Pacer
{
public:
    Pacer(IFFmpegRenderer* renderer);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    bool initialize(SDL_Window* window, int maxVideoFps);

    void vsyncCallback();

    bool isUsingFrameQueue();

private:
    QQueue<AVFrame*> m_FrameQueue;
    QQueue<int> m_FrameQueueHistory;
    SDL_SpinLock m_FrameQueueLock;

    IVsyncSource* m_VsyncSource;
    IFFmpegRenderer* m_VsyncRenderer;
    int m_MaxVideoFps;
    int m_DisplayFps;
};
