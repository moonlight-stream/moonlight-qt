// micstream.cpp - Client-side microphone passthrough sender.
//
// Captures the default microphone with Qt, encodes Opus (VOIP, 64 kbps,
// 20 ms frames), and sends each frame to the host as a raw control-stream
// packet of type 0x3003 (the VibePollo mic protocol), via
// LiSendRawControlStreamPacket(). The wire format is a 4-byte header
// [seq_hi, seq_lo, channel=1, flags=0] followed by the Opus payload.
//
// This is adapted from qiin2333's Moonlight V+ MicStream (Qt capture +
// Opus worker design, bounded queues that drop frames instead of building
// latency), rewired from their UDP mic transport to the 0x3003 control
// packet. Unlike qiin2333's design, no mic negotiation is needed: the
// host's 0x3003 handler is unconditional, and the client simply sends
// while a session is active and the user enabled the microphone.
//
// NOTE: LiSendRawControlStreamPacket() does not exist in upstream
// moonlight-common-c. It lives in neer2005/moonlight-common-c (ported
// from xenstalker02's fork) and moonlight-qt's submodule must point
// there before this builds.

#include "micstream.h"
#include "macpermissions.h"

#include <cstdint>
#include <cstring>

#include <Limelight.h>
#include <opus.h>
#include <QAudio>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QDebug>
#include <QElapsedTimer>
#include <QIODevice>
#include <QMediaDevices>
#include <QQueue>
#include <QThread>
#include <QTimer>

static const int PCM_FRAME_SAMPLES = 960; // 20 ms at 48 kHz
static const int PCM_FRAME_SIZE = PCM_FRAME_SAMPLES * 2; // mono 16-bit
static const int MAX_OPUS_SIZE = 4000;
static const int MAX_PENDING_PCM_FRAMES = 10;
static const int MAX_PENDING_OPUS_FRAMES = 10;
static const int MAX_ENCODE_FRAMES_PER_PASS = 4;
static const int MAX_SEND_FRAMES_PER_PASS = 4;

// VibePollo mic packet type on the control stream (see host stream.cpp).
static const uint16_t MIC_PACKET_TYPE = 0x3003;
// Max Opus payload per packet. The host accepts 4..4096-byte payloads,
// but the control-stream send path keeps packets small; 64 kbps at
// 20 ms is ~160 bytes/frame, comfortably under this cap. Frames that
// encode larger are dropped rather than split.
static const int MAX_OPUS_SEND_SIZE = 248;
static const int MIC_HEADER_SIZE = 4;

class MicStreamWorker : public QObject
{
public:
    MicStreamWorker()
        : m_audioInput(nullptr),
          m_audioDevice(nullptr),
          m_encoder(nullptr),
          m_sendTimer(nullptr),
          m_logTimer(nullptr),
          m_audioContinuationPending(false),
          m_micSeq(0),
          m_pcmBytes(0),
          m_opusBytes(0),
          m_sentBytes(0),
          m_sentPackets(0),
          m_sendErrors(0),
          m_idleLoops(0),
          m_droppedPcmFrames(0),
          m_droppedOpusFrames(0),
          m_droppedOversizedFrames(0),
          m_maxAudioCallbackMs(0)
    {
    }

    bool start()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_audioInput)
            return false;

        int err;
        m_encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_VOIP, &err);
        if (err != OPUS_OK) {
            m_encoder = nullptr;
            return false;
        }
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(64000));

        QAudioFormat fmt;
        fmt.setSampleRate(48000);
        fmt.setChannelCount(1);
        fmt.setSampleFormat(QAudioFormat::Int16);

        QAudioDevice device = QMediaDevices::defaultAudioInput();
        if (device.isNull()) {
            qWarning() << "[MicStream] No default audio input device available";
            cleanupAudioResources();
            return false;
        }

        if (!device.isFormatSupported(fmt)) {
            const QAudioFormat preferredFormat = device.preferredFormat();
            qWarning() << "[MicStream] Default audio input does not support required format: 48000 Hz mono Int16"
                       << "device=" << device.description()
                       << "preferred=" << preferredFormat.sampleRate() << "Hz"
                       << preferredFormat.channelCount() << "channels"
                       << "sampleFormat=" << preferredFormat.sampleFormat();
            cleanupAudioResources();
            return false;
        }

        qInfo() << "[MicStream] Using audio input device:" << device.description()
                << "format=" << fmt.sampleRate() << "Hz" << fmt.channelCount() << "channels";

        m_audioInput = new QAudioSource(device, fmt, this);
        m_audioInput->setBufferSize(PCM_FRAME_SIZE * 4);
        connect(m_audioInput, &QAudioSource::stateChanged, this, [this](QAudio::State state) {
            qInfo() << "[MicStream] Audio state changed state=" << state
                    << "error=" << m_audioInput->error();
        });

        m_audioDevice = m_audioInput->start();
        if (!m_audioDevice || m_audioInput->error() != QAudio::NoError) {
            qWarning() << "[MicStream] Failed to start audio device error=" << m_audioInput->error();
            cleanupAudioResources();
            return false;
        }

        qInfo() << "[MicStream] Audio device initialized successfully";
        connect(m_audioDevice, &QIODevice::readyRead, this, &MicStreamWorker::onAudio);

        resetStatistics();
        m_micSeq = 0;
        m_partialBuffer.clear();
        m_queue.clear();
        m_audioContinuationPending = false;

        m_sendTimer = new QTimer(this);
        m_sendTimer->setInterval(20);
        connect(m_sendTimer, &QTimer::timeout, this, &MicStreamWorker::sendLoop);
        m_sendTimer->start();

        m_logTimer = new QTimer(this);
        m_logTimer->setInterval(5000);
        connect(m_logTimer, &QTimer::timeout, this, &MicStreamWorker::logSummary);
        m_logTimer->start();

        qInfo() << "[MicStream] start thread=" << QThread::currentThread()->objectName();
        return true;
    }

    void stop()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!m_audioInput && !m_encoder)
            return;

        if (m_sendTimer)
            m_sendTimer->stop();
        if (m_logTimer)
            m_logTimer->stop();
        logSummary();
        cleanupAudioResources();

        m_partialBuffer.clear();
        m_queue.clear();
        m_audioContinuationPending = false;
        qInfo() << "[MicStream] stop";
    }

private:
    void cleanupAudioResources()
    {
        if (m_audioInput) {
            m_audioInput->stop();
            delete m_audioInput;
            m_audioInput = nullptr;
            m_audioDevice = nullptr;
        }
        if (m_encoder) {
            opus_encoder_destroy(m_encoder);
            m_encoder = nullptr;
        }
    }

    void onAudio()
    {
        if (!m_audioDevice || !m_encoder)
            return;

        QElapsedTimer callbackTimer;
        callbackTimer.start();

        QByteArray chunk = m_audioDevice->readAll();
        if (!chunk.isEmpty()) {
            m_partialBuffer.append(chunk);
            trimPcmBacklog();
            processPendingAudio();
        }

        m_maxAudioCallbackMs = qMax(m_maxAudioCallbackMs, callbackTimer.elapsed());
    }

    void trimPcmBacklog()
    {
        const int maxBytes = MAX_PENDING_PCM_FRAMES * PCM_FRAME_SIZE;
        if (m_partialBuffer.size() <= maxBytes)
            return;

        const int excessBytes = m_partialBuffer.size() - maxBytes;
        const int framesToDrop = (excessBytes + PCM_FRAME_SIZE - 1) / PCM_FRAME_SIZE;
        const int completeBytes = m_partialBuffer.size() - (m_partialBuffer.size() % PCM_FRAME_SIZE);
        const int bytesToDrop = qMin(framesToDrop * PCM_FRAME_SIZE, completeBytes);
        m_partialBuffer.remove(0, bytesToDrop);
        m_droppedPcmFrames += bytesToDrop / PCM_FRAME_SIZE;
    }

    void processPendingAudio()
    {
        if (!m_encoder)
            return;

        int processedFrames = 0;
        while (m_partialBuffer.size() >= PCM_FRAME_SIZE &&
               processedFrames < MAX_ENCODE_FRAMES_PER_PASS) {
            QByteArray pcm = m_partialBuffer.left(PCM_FRAME_SIZE);
            m_partialBuffer.remove(0, PCM_FRAME_SIZE);
            m_pcmBytes += pcm.size();

            unsigned char encoded[MAX_OPUS_SIZE];
            int len = opus_encode(m_encoder,
                                  reinterpret_cast<const opus_int16*>(pcm.constData()),
                                  PCM_FRAME_SAMPLES,
                                  encoded,
                                  MAX_OPUS_SIZE);
            if (len > 0) {
                while (m_queue.size() >= MAX_PENDING_OPUS_FRAMES) {
                    m_queue.dequeue();
                    m_droppedOpusFrames++;
                }
                m_queue.enqueue(QByteArray(reinterpret_cast<char*>(encoded), len));
                m_opusBytes += len;
            } else {
                qWarning() << "[MicStream] opus_encode failed len=" << len;
            }
            processedFrames++;
        }

        if (m_partialBuffer.size() >= PCM_FRAME_SIZE && !m_audioContinuationPending) {
            m_audioContinuationPending = true;
            QTimer::singleShot(0, this, [this]() {
                m_audioContinuationPending = false;
                processPendingAudio();
            });
        }
    }

    void sendLoop()
    {
        if (m_queue.isEmpty()) {
            m_idleLoops++;
            return;
        }

        int processedFrames = 0;
        while (!m_queue.isEmpty() && processedFrames < MAX_SEND_FRAMES_PER_PASS) {
            QByteArray opus = m_queue.dequeue();
            int opusLen = opus.size();

            // The control-stream packet must stay small. Drop oversized
            // frames rather than splitting them; at 64 kbps this should
            // effectively never trigger (~160 bytes/frame typical).
            if (opusLen > MAX_OPUS_SEND_SIZE) {
                m_droppedOversizedFrames++;
                processedFrames++;
                continue;
            }

            unsigned char packet[MIC_HEADER_SIZE + MAX_OPUS_SEND_SIZE];
            uint16_t seq = m_micSeq++;
            packet[0] = static_cast<unsigned char>((seq >> 8) & 0xFF);
            packet[1] = static_cast<unsigned char>(seq & 0xFF);
            packet[2] = 1; // channel: mono
            packet[3] = 0; // flags
            memcpy(packet + MIC_HEADER_SIZE, opus.constData(), opusLen);

            int rc = LiSendRawControlStreamPacket(MIC_PACKET_TYPE, packet,
                                                  MIC_HEADER_SIZE + opusLen);
            if (rc != 0) {
                // Don't flood the log at 50 packets/sec; the 5s summary
                // reports the running error count.
                if (m_sendErrors == 0) {
                    qWarning() << "[MicStream] LiSendRawControlStreamPacket failed rc=" << rc;
                }
                m_sendErrors++;
            } else {
                m_sentPackets++;
                m_sentBytes += opusLen;
            }
            processedFrames++;
        }
    }

    void logSummary()
    {
        const int state = m_audioInput ? static_cast<int>(m_audioInput->state()) : -1;
        const int error = m_audioInput ? static_cast<int>(m_audioInput->error()) : -1;
        qInfo() << "[MicStream] 5s summary pcm=" << m_pcmBytes
                << "B opus=" << m_opusBytes
                << "B sent=" << m_sentPackets << "/" << m_sentBytes
                << "B sendErr=" << m_sendErrors
                << "B idle=" << m_idleLoops
                << "queue=" << m_queue.size()
                << "pcmDrop=" << m_droppedPcmFrames
                << "opusDrop=" << m_droppedOpusFrames
                << "oversizeDrop=" << m_droppedOversizedFrames
                << "maxAudioMs=" << m_maxAudioCallbackMs
                << "state=" << state
                << "error=" << error;
        resetStatistics();
    }

    void resetStatistics()
    {
        m_pcmBytes = 0;
        m_opusBytes = 0;
        m_sentBytes = 0;
        m_sentPackets = 0;
        m_sendErrors = 0;
        m_idleLoops = 0;
        m_droppedPcmFrames = 0;
        m_droppedOpusFrames = 0;
        m_droppedOversizedFrames = 0;
        m_maxAudioCallbackMs = 0;
    }

    QAudioSource *m_audioInput;
    QIODevice *m_audioDevice;
    OpusEncoder *m_encoder;
    QTimer *m_sendTimer;
    QTimer *m_logTimer;
    QQueue<QByteArray> m_queue;
    bool m_audioContinuationPending;
    uint16_t m_micSeq;
    quint64 m_pcmBytes;
    quint64 m_opusBytes;
    quint64 m_sentBytes;
    int m_sentPackets;
    int m_sendErrors;
    int m_idleLoops;
    int m_droppedPcmFrames;
    int m_droppedOpusFrames;
    int m_droppedOversizedFrames;
    qint64 m_maxAudioCallbackMs;
    QByteArray m_partialBuffer;
};

MicStream::MicStream(QObject *parent)
    : QObject(parent),
      m_thread(nullptr),
      m_worker(nullptr)
{
}

MicStream::~MicStream()
{
    stop();
}

bool MicStream::start()
{
    if (m_thread)
        return false;

    // Keep the macOS permission request on the application thread. Touching
    // QMediaDevices and creating QAudioSource happens in the worker thread.
    if (!checkAndRequestMicrophonePermission()) {
        qWarning() << "[MicStream] Microphone permission denied";
        return false;
    }

    QThread *thread = new QThread(this);
    thread->setObjectName("Microphone Streaming Thread");
    MicStreamWorker *worker = new MicStreamWorker();
    worker->moveToThread(thread);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    thread->start();

    bool started = false;
    const bool invoked = QMetaObject::invokeMethod(worker, [worker, &started]() {
        started = worker->start();
    }, Qt::BlockingQueuedConnection);

    if (!invoked || !started) {
        qWarning() << "[MicStream] Failed to start microphone worker";
        thread->quit();
        thread->wait();
        delete thread;
        return false;
    }

    m_thread = thread;
    m_worker = worker;
    return true;
}

void MicStream::stop()
{
    if (!m_thread)
        return;

    Q_ASSERT(QThread::currentThread() != m_thread);
    if (m_worker && m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
            worker->stop();
        }, Qt::BlockingQueuedConnection);
    }

    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_worker = nullptr;
}
