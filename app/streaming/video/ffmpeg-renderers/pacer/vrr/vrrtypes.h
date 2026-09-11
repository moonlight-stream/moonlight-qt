#pragma once

// The VRR pacing path deliberately keeps its data contract separate from
// FFmpeg's PTS/DTS fields.  Those fields are decoder-owned and are still used
// by the legacy pacing path, while these values describe the frame as it
// crossed the decoder/pacer boundary.

#include <cstdint>
#include <memory>

extern "C" {
#include <libavutil/frame.h>
}

struct VrrSessionConfig {
    int displayRefreshHz = 0;
    int streamRateHz = 0;
};

// A move-only frame record.  Decoder completion is captured while the
// corresponding DECODE_UNIT is still available.  A raw RTP timestamp of zero
// is valid; timestampValid is intentionally separate to make that explicit.
class PacedFrame {
public:
    PacedFrame() = default;

    PacedFrame(AVFrame* frame,
               int frameNumber,
               uint32_t rtpTimestamp,
               bool timestampValid,
               uint64_t decodeCompleteUs) :
        m_Frame(frame),
        m_FrameNumber(frameNumber),
        m_RtpTimestamp(rtpTimestamp),
        m_TimestampValid(timestampValid),
        m_DecodeCompleteUs(decodeCompleteUs)
    {
    }

    PacedFrame(PacedFrame&&) noexcept = default;
    PacedFrame& operator=(PacedFrame&&) noexcept = default;

    AVFrame* frame() const
    {
        return m_Frame.get();
    }

    AVFrame* release()
    {
        return m_Frame.release();
    }

    explicit operator bool() const
    {
        return m_Frame != nullptr;
    }

    int frameNumber() const
    {
        return m_FrameNumber;
    }

    uint32_t rtpTimestamp() const
    {
        return m_RtpTimestamp;
    }

    bool timestampValid() const
    {
        return m_TimestampValid;
    }

    uint64_t decodeCompleteUs() const
    {
        return m_DecodeCompleteUs;
    }

private:
    struct FrameDeleter {
        void operator()(AVFrame* frame) const
        {
            av_frame_free(&frame);
        }
    };

    std::unique_ptr<AVFrame, FrameDeleter> m_Frame;
    int m_FrameNumber = -1;
    uint32_t m_RtpTimestamp = 0;
    bool m_TimestampValid = false;
    uint64_t m_DecodeCompleteUs = 0;
};
