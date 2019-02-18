#pragma once

#include "../../decoder.h"
#include "../renderer.h"

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

class IVsyncSource {
public:
    virtual ~IVsyncSource() {}
    virtual bool initialize(SDL_Window* window, int displayFps) = 0;
};

class Pacer
{
public:
    Pacer(IFFmpegRenderer* renderer, PVIDEO_STATS videoStats);

    ~Pacer();

    void submitFrame(AVFrame* frame);

    bool initialize(SDL_Window* window, int maxVideoFps, bool enablePacing);

    void vsyncCallback(int timeUntilNextVsyncMillis);

private:
    static int renderThread(void* context);

    void renderFrame(AVFrame* frame);

    void dropFrameForEnqueue(QQueue<AVFrame*>& queue);

    QQueue<AVFrame*> m_RenderQueue;
    QQueue<AVFrame*> m_PacingQueue;
    QQueue<int> m_PacingQueueHistory;
    QQueue<int> m_RenderQueueHistory;
    QMutex m_FrameQueueLock;
    QWaitCondition m_RenderQueueNotEmpty;
    QWaitCondition m_PacingQueueNotEmpty;
    SDL_Thread* m_RenderThread;
    bool m_Stopping;

    IVsyncSource* m_VsyncSource;
    IFFmpegRenderer* m_VsyncRenderer;
    int m_MaxVideoFps;
    int m_DisplayFps;
    PVIDEO_STATS m_VideoStats;
};
