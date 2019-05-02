#include "slvid.h"

SLVideoDecoder::SLVideoDecoder(bool)
    : m_VideoContext(nullptr),
      m_VideoStream(nullptr)
{
    SLVideo_SetLogFunction(SLVideoDecoder::slLogCallback, nullptr);
}

SLVideoDecoder::~SLVideoDecoder()
{
    if (m_VideoStream != nullptr) {
        SLVideo_FreeStream(m_VideoStream);
    }

    if (m_VideoContext != nullptr) {
        SLVideo_FreeContext(m_VideoContext);
    }
}

bool
SLVideoDecoder::isHardwareAccelerated()
{
    // SLVideo is always hardware accelerated
    return true;
}

int
SLVideoDecoder::getDecoderCapabilities()
{
    return 0;
}

bool
SLVideoDecoder::initialize(PDECODER_PARAMETERS params)
{
    // SLVideo only supports hardware decoding
    if (params->vds == StreamingPreferences::VDS_FORCE_SOFTWARE) {
        return false;
    }

    // SLVideo only supports H.264
    if (params->videoFormat != VIDEO_FORMAT_H264) {
        return false;
    }

    m_VideoContext = SLVideo_CreateContext();
    if (!m_VideoContext) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SLVideo_CreateContext() failed");
        return false;
    }

    // Create a low latency H.264 stream
    m_VideoStream = SLVideo_CreateStream(m_VideoContext, k_ESLVideoFormatH264, 1);
    if (!m_VideoStream) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SLVideo_CreateStream() failed");
        return false;
    }

    SLVideo_SetStreamTargetFramerate(m_VideoStream, params->frameRate, 1);

    return true;
}

int
SLVideoDecoder::submitDecodeUnit(PDECODE_UNIT du)
{
    int err;

    err = SLVideo_BeginFrame(m_VideoStream, du->fullLength);
    if (err < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SLVideo_BeginFrame() failed: %d",
                    err);

        // Need an IDR frame to resync
        return DR_NEED_IDR;
    }

    PLENTRY entry = du->bufferList;
    while (entry != nullptr) {
        err = SLVideo_WriteFrameData(m_VideoStream,
                                     entry->data,
                                     entry->length);
        if (err < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "SLVideo_WriteFrameData() failed: %d",
                        err);

            // Need an IDR frame to resync
            return DR_NEED_IDR;
        }

        entry = entry->next;
    }

    err = SLVideo_SubmitFrame(m_VideoStream);
    if (err < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "SLVideo_SubmitFrame() failed: %d",
                    err);

        // Need an IDR frame to resync
        return DR_NEED_IDR;
    }

    return DR_OK;
}

void SLVideoDecoder::slLogCallback(void*, ESLVideoLog logLevel, const char *message)
{
    SDL_LogPriority priority;

    switch (logLevel)
    {
    case k_ESLVideoLogError:
        priority = SDL_LOG_PRIORITY_ERROR;
        break;
    case k_ESLVideoLogWarning:
        priority = SDL_LOG_PRIORITY_WARN;
        break;
    case k_ESLVideoLogInfo:
        priority = SDL_LOG_PRIORITY_INFO;
        break;
    default:
    case k_ESLVideoLogDebug:
        priority = SDL_LOG_PRIORITY_DEBUG;
        break;
    }

    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION,
                   priority,
                   "SLVideo: %s",
                   message);
}
