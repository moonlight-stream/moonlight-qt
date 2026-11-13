#include "adaptivebitrateservice.h"

#include "backend/nvhttp.h"

#include <QMetaObject>
#include <QDateTime>

AdaptiveBitrateService::AdaptiveBitrateService(NvHttpFactory nvHttpFactory,
                                               StatsProvider statsProvider,
                                               BitrateChangedCallback onBitrateChanged,
                                               QObject* parent) :
    QObject(parent),
    m_NvHttpFactory(std::move(nvHttpFactory)),
    m_StatsProvider(std::move(statsProvider)),
    m_OnBitrateChanged(std::move(onBitrateChanged)),
    m_NvHttp(nullptr),
    m_Enabled(false),
    m_ServerSupported(false),
    m_CurrentBitrate(0),
    m_InitialBitrate(0),
    m_Mode(StreamingPreferences::AbrMode::ABR_BALANCED),
    m_MinBitrate(3000),
    m_MaxBitrate(100000),
    m_StableSeconds(0),
    m_LossStreak(0),
    m_LastAdjustWallClock(0),
    m_LastDirection(k_DirNone),
    m_ServerEnableRetries(0),
    m_ServerRetryTickCounter(0),
    m_TickCount(0)
{
    m_Timer.setInterval(1000);
    m_Timer.setSingleShot(false);
    connect(&m_Timer, &QTimer::timeout, this, &AdaptiveBitrateService::tick);

    moveToThread(&m_WorkerThread);
    m_WorkerThread.start();
}

AdaptiveBitrateService::~AdaptiveBitrateService()
{
    if (m_Enabled) {
        QMetaObject::invokeMethod(this, [this]() {
            m_Timer.stop();
            if (m_NvHttp != nullptr) {
                if (m_ServerSupported) {
                    m_NvHttp->setAbrMode({false, 0, 0, abrModeToString(StreamingPreferences::AbrMode::ABR_BALANCED)});
                }
                if (m_CurrentBitrate != m_InitialBitrate) {
                    applyBitrateInternal(m_NvHttp, m_InitialBitrate, QStringLiteral("restore"), QStringLiteral("local"));
                }
            }
            m_Enabled = false;
        }, Qt::BlockingQueuedConnection);
    }

    m_WorkerThread.quit();
    m_WorkerThread.wait();
}

QString AdaptiveBitrateService::abrModeToString(StreamingPreferences::AbrMode mode)
{
    switch (mode) {
    case StreamingPreferences::AbrMode::ABR_QUALITY:
        return QStringLiteral("quality");
    case StreamingPreferences::AbrMode::ABR_LOW_LATENCY:
        return QStringLiteral("lowLatency");
    case StreamingPreferences::AbrMode::ABR_BALANCED:
    default:
        return QStringLiteral("balanced");
    }
}

void AdaptiveBitrateService::start(int initialBitrateKbps, StreamingPreferences::AbrMode mode)
{
    QMetaObject::invokeMethod(this, [this, initialBitrateKbps, mode]() {
        if (m_Enabled) {
            return;
        }

        m_InitialBitrate = initialBitrateKbps;
        m_CurrentBitrate = initialBitrateKbps;
        m_Mode = mode;
        applyModePreset(mode);
        resetState();
        m_Enabled = true;
        m_TickCount = 0;

        if (m_NvHttp == nullptr) {
            m_NvHttp = m_NvHttpFactory();
        }

        QTimer::singleShot(k_StartDelaySeconds * 1000, this, &AdaptiveBitrateService::probeServerCapabilities);
        m_Timer.start();

        qInfo() << "[ABR] Starting:" << initialBitrateKbps << "kbps, mode:" << abrModeToString(mode);
    }, Qt::QueuedConnection);
}

void AdaptiveBitrateService::stop()
{
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_Enabled) {
            return;
        }

        m_Enabled = false;
        m_Timer.stop();

        if (m_NvHttp != nullptr) {
            if (m_ServerSupported) {
                m_NvHttp->setAbrMode({false, 0, 0, abrModeToString(StreamingPreferences::AbrMode::ABR_BALANCED)});
            }
            if (m_CurrentBitrate != m_InitialBitrate) {
                applyBitrateInternal(m_NvHttp, m_InitialBitrate, QStringLiteral("restore"), QStringLiteral("local"));
            }
        }

        qInfo() << "[ABR] Stopped, restored bitrate:" << m_InitialBitrate << "kbps";

        delete m_NvHttp;
        m_NvHttp = nullptr;
    }, Qt::BlockingQueuedConnection);
}

QString AdaptiveBitrateService::statusText() const
{
    if (!m_Enabled) {
        return QString();
    }

    QString sub;
    if (m_ServerSupported && m_ServerEnableRetries == 0) {
        sub = QStringLiteral("server");
    }
    else if (m_ServerSupported && m_ServerEnableRetries > 0) {
        sub = QStringLiteral("connecting");
    }
    else {
        sub = QStringLiteral("local");
    }

    return QStringLiteral("ABR:%1 %2M").arg(sub).arg(m_CurrentBitrate / 1000);
}

void AdaptiveBitrateService::probeServerCapabilities()
{
    if (!m_Enabled || m_NvHttp == nullptr) {
        return;
    }

    try {
        AbrCapabilities caps = m_NvHttp->getAbrCapabilities();
        m_ServerSupported = caps.supported;
        if (caps.supported) {
            m_ServerEnableRetries = 1;
        }
        qInfo() << "[ABR] Server capabilities:" << caps.supported << "version:" << caps.version;
    }
    catch (const std::exception& e) {
        qWarning() << "[ABR] Capability probe failed:" << e.what();
    }
}

void AdaptiveBitrateService::applyModePreset(StreamingPreferences::AbrMode mode)
{
    switch (mode) {
    case StreamingPreferences::AbrMode::ABR_QUALITY:
        m_MinBitrate = qMax(5000, static_cast<int>(m_InitialBitrate * 0.5));
        m_MaxBitrate = qMin(150000, static_cast<int>(m_InitialBitrate * 1.5));
        break;
    case StreamingPreferences::AbrMode::ABR_LOW_LATENCY:
        m_MinBitrate = 2000;
        m_MaxBitrate = static_cast<int>(m_InitialBitrate * 1.2);
        break;
    case StreamingPreferences::AbrMode::ABR_BALANCED:
    default:
        m_MinBitrate = qMax(3000, static_cast<int>(m_InitialBitrate * 0.3));
        m_MaxBitrate = qMin(150000, m_InitialBitrate * 2);
        break;
    }
}

void AdaptiveBitrateService::resetState()
{
    m_StableSeconds = 0;
    m_LossStreak = 0;
    m_LastAdjustWallClock = 0;
    m_LastDirection = k_DirNone;
    m_ServerEnableRetries = 0;
    m_ServerRetryTickCounter = 0;
}

void AdaptiveBitrateService::tick()
{
    if (!m_Enabled || m_NvHttp == nullptr) {
        return;
    }

    if (m_TickCount++ < k_StartDelaySeconds) {
        return;
    }

    auto stats = m_StatsProvider();
    if (!stats.has_value()) {
        return;
    }

    if (m_ServerSupported && m_ServerEnableRetries > 0) {
        if (++m_ServerRetryTickCounter >= k_ServerRetryIntervalTicks) {
            m_ServerRetryTickCounter = 0;
            AbrConfig config;
            config.enabled = true;
            config.minBitrate = m_MinBitrate;
            config.maxBitrate = m_MaxBitrate;
            config.mode = abrModeToString(m_Mode);
            if (m_NvHttp->setAbrMode(config)) {
                qInfo() << "[ABR] Server ABR enabled on retry" << m_ServerEnableRetries;
                m_ServerEnableRetries = 0;
            }
            else if (++m_ServerEnableRetries > k_MaxServerEnableRetries) {
                m_ServerSupported = false;
                qWarning() << "[ABR] Server enable failed after" << k_MaxServerEnableRetries << "retries, using local controller";
            }
        }
    }

    if (m_ServerSupported && m_ServerEnableRetries == 0) {
        tickServer(m_NvHttp, stats.value());
    }
    else {
        tickLocal(stats.value());
    }
}

void AdaptiveBitrateService::tickServer(NvHTTP* http, const AbrStats& stats)
{
    NetworkFeedback feedback;
    feedback.packetLoss = stats.packetLoss;
    feedback.rttMs = stats.rttMs;
    feedback.decodeFps = stats.decodeFps;
    feedback.droppedFrames = stats.droppedFrames;
    feedback.currentBitrate = m_CurrentBitrate;

    std::optional<AbrAction> action = http->reportNetworkFeedback(feedback);
    if (!action.has_value() || !action->newBitrate.has_value()) {
        return;
    }

    int newBitrate = action->newBitrate.value();
    if (newBitrate == m_CurrentBitrate) {
        return;
    }

    int clamped = qBound(m_MinBitrate, newBitrate, m_MaxBitrate);
    applyBitrateInternal(http, clamped, action->reason.value_or(QStringLiteral("server")), QStringLiteral("server"));
}

bool AdaptiveBitrateService::applyBitrateInternal(NvHTTP* http, int kbps, const QString& reason, const QString& source)
{
    int from = m_CurrentBitrate;
    try {
        if (http->setBitrate(kbps)) {
            m_CurrentBitrate = kbps;
            qInfo() << "[ABR][" << source << "]" << from << "kbps ->" << kbps << "kbps (" << reason << ")";
            if (m_OnBitrateChanged) {
                m_OnBitrateChanged(kbps, reason);
            }
            return true;
        }
    }
    catch (const std::exception& e) {
        qWarning() << "[ABR] setBitrate failed:" << e.what();
    }
    return false;
}

void AdaptiveBitrateService::tickLocal(const AbrStats& stats)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    int cooldown = (m_Mode == StreamingPreferences::AbrMode::ABR_LOW_LATENCY) ? 1500 : 2000;
    if (now - m_LastAdjustWallClock < cooldown) {
        return;
    }

    double newBitrate = m_CurrentBitrate;
    QString reason;

    if (stats.packetLoss > 5.0f) {
        newBitrate = m_CurrentBitrate * 0.7;
        reason = QStringLiteral("loss=%1% emergency").arg(stats.packetLoss, 0, 'f', 1);
        m_StableSeconds = 0;
        m_LossStreak++;
    }
    else if (stats.packetLoss > 2.0f) {
        m_LossStreak++;
        if (m_LossStreak >= 2) {
            newBitrate = m_CurrentBitrate * 0.9;
            reason = QStringLiteral("loss=%1% sustained").arg(stats.packetLoss, 0, 'f', 1);
            m_StableSeconds = 0;
        }
    }
    else if (stats.packetLoss > 0.5f) {
        m_LossStreak++;
        m_StableSeconds = 0;
        if (m_LossStreak >= 4) {
            newBitrate = m_CurrentBitrate * 0.95;
            reason = QStringLiteral("loss=%1% mild").arg(stats.packetLoss, 0, 'f', 1);
        }
    }
    else {
        m_LossStreak = 0;
        m_StableSeconds++;
        int probeThreshold = (m_Mode == StreamingPreferences::AbrMode::ABR_QUALITY) ? 3 : 5;
        if (m_StableSeconds >= probeThreshold && m_CurrentBitrate < m_MaxBitrate) {
            double step = (m_LastDirection == k_DirDown) ? 1.02 : 1.05;
            newBitrate = m_CurrentBitrate * step;
            reason = QStringLiteral("stable %1s probe").arg(m_StableSeconds);
            m_StableSeconds = 0;
        }
    }

    int target = qBound(m_MinBitrate, static_cast<int>(newBitrate), m_MaxBitrate);
    if (target != m_CurrentBitrate) {
        int direction = (target > m_CurrentBitrate) ? k_DirUp : k_DirDown;
        if (applyBitrateInternal(m_NvHttp, target, reason, QStringLiteral("local"))) {
            m_LastDirection = direction;
            m_LastAdjustWallClock = now;
        }
    }
}
