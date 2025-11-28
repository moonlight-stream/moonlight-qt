/**
 * @file app/streaming/video/ffmpeg-renderers/swframemapper.h
 * @brief Software frame mapper for converting hardware frames to software frames.
 */

#pragma once

#include "renderer.h"

/**
 * @brief Maps hardware-accelerated video frames to software frames for processing.
 */
class SwFrameMapper
{
public:
    explicit SwFrameMapper(IFFmpegRenderer* renderer);
    void setVideoFormat(int videoFormat);
    AVFrame* getSwFrameFromHwFrame(AVFrame* hwFrame);

private:
    bool initializeReadBackFormat(AVBufferRef* hwFrameCtxRef, AVFrame* testFrame);

    IFFmpegRenderer* m_Renderer;
    int m_VideoFormat;
    enum AVPixelFormat m_SwPixelFormat;
    bool m_MapFrame;
};
