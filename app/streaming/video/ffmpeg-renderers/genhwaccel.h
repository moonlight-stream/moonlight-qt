/**
 * @file app/streaming/video/ffmpeg-renderers/genhwaccel.h
 * @brief Generic hardware-accelerated video renderer implementation.
 */

#pragma once

#include "renderer.h"

/**
 * @brief Generic hardware-accelerated video renderer supporting various FFmpeg hardware device types.
 */
class GenericHwAccelRenderer : public IFFmpegRenderer
{
public:
    GenericHwAccelRenderer(AVHWDeviceType hwDeviceType);
    virtual ~GenericHwAccelRenderer() override;
    virtual bool initialize(PDECODER_PARAMETERS) override;
    virtual bool prepareDecoderContext(AVCodecContext* context, AVDictionary** options) override;
    virtual void renderFrame(AVFrame* frame) override;
    virtual bool needsTestFrame() override;
    virtual bool isDirectRenderingSupported() override;
    virtual int getDecoderCapabilities() override;

private:
    AVHWDeviceType m_HwDeviceType;
    AVBufferRef* m_HwContext;
};

