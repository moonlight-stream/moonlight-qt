#pragma once

#include "renderer.h"

#include <QQueue>

class Pacer
{
public:
    Pacer();

    ~Pacer();

    AVFrame* getFrameAtVsync();

    void submitFrame(AVFrame* frame);

    void drain();

private:
    QQueue<AVFrame*> m_FrameQueue;
    QQueue<int> m_FrameQueueHistory;
    SDL_SpinLock m_FrameQueueLock;
};
