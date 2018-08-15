#include "pacer.h"

#define FRAME_HISTORY_ENTRIES 8

Pacer::Pacer() :
    m_FrameQueueLock(0)
{

}

Pacer::~Pacer()
{
    drain();
}

AVFrame* Pacer::getFrameAtVsync()
{
    SDL_AtomicLock(&m_FrameQueueLock);

    int frameDropTarget;

    // If the queue length history entries are large, be strict
    // about dropping excess frames.
    frameDropTarget = 1;
    for (int i = 0; i < m_FrameQueueHistory.count(); i++) {
        if (m_FrameQueueHistory[i] <= 1) {
            // Be lenient as long as the queue length
            // resolves before the end of frame history
            frameDropTarget = 3;
        }
    }

    if (m_FrameQueueHistory.count() == FRAME_HISTORY_ENTRIES) {
        m_FrameQueueHistory.dequeue();
    }

    m_FrameQueueHistory.enqueue(m_FrameQueue.count());

    // Catch up if we're several frames ahead
    while (m_FrameQueue.count() > frameDropTarget) {
        AVFrame* frame = m_FrameQueue.dequeue();
        av_frame_free(&frame);
    }

    if (m_FrameQueue.isEmpty()) {
        SDL_AtomicUnlock(&m_FrameQueueLock);
        return nullptr;
    }

    // Grab the first frame
    AVFrame* frame = m_FrameQueue.dequeue();
    SDL_AtomicUnlock(&m_FrameQueueLock);

    return frame;
}

void Pacer::submitFrame(AVFrame* frame)
{
    SDL_AtomicLock(&m_FrameQueueLock);
    m_FrameQueue.enqueue(frame);
    SDL_AtomicUnlock(&m_FrameQueueLock);
}

void Pacer::drain()
{
    while (!m_FrameQueue.isEmpty()) {
        AVFrame* frame = m_FrameQueue.dequeue();
        av_frame_free(&frame);
    }
}
