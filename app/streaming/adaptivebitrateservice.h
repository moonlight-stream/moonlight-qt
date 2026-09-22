#pragma once

#include "settings/streamingpreferences.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>
#include <optional>

class NvHTTP;

class AdaptiveBitrateService : public QObject
{
    Q_OBJECT

public:
    struct AbrStats {
        float packetLoss;
        int rttMs;
        float decodeFps;
        int droppedFrames;
    };

    using NvHttpFactory = std::function<NvHTTP*()>;
    using StatsProvider = std::function<std::optional<AbrStats>()>;
    using BitrateChangedCallback = std::function<void(int bitrateKbps, const QString& reason)>;

    AdaptiveBitrateService(NvHttpFactory nvHttpFactory,
                           StatsProvider statsProvider,
                           BitrateChangedCallback onBitrateChanged,
                           QObject* parent = nullptr);
    ~AdaptiveBitrateService() override;

    void start(int initialBitrateKbps, StreamingPreferences::AbrMode mode);
    void stop();

    bool isEnabled() const
    {
        return m_Enabled;
    }

    QString statusText() const;

private slots:
    void probeServerCapabilities();
    void tick();

private:
    void applyModePreset(StreamingPreferences::AbrMode mode);
    void resetState();
    void tickServer(NvHTTP* http, const AbrStats& stats);
    void tickLocal(const AbrStats& stats);
    bool applyBitrateInternal(NvHTTP* http, int kbps, const QString& reason, const QString& source);
    static QString abrModeToString(StreamingPreferences::AbrMode mode);

    NvHttpFactory m_NvHttpFactory;
    StatsProvider m_StatsProvider;
    BitrateChangedCallback m_OnBitrateChanged;

    QThread m_WorkerThread;
    QTimer m_Timer;

    NvHTTP* m_NvHttp;
    bool m_Enabled;
    bool m_ServerSupported;
    int m_CurrentBitrate;
    int m_InitialBitrate;
    StreamingPreferences::AbrMode m_Mode;
    int m_MinBitrate;
    int m_MaxBitrate;

    int m_StableSeconds;
    int m_LossStreak;
    qint64 m_LastAdjustWallClock;
    int m_LastDirection;

    int m_ServerEnableRetries;
    int m_ServerRetryTickCounter;
    int m_TickCount;

    static constexpr int k_StartDelaySeconds = 3;
    static constexpr int k_ServerRetryIntervalTicks = 5;
    static constexpr int k_MaxServerEnableRetries = 10;
    static constexpr int k_DirNone = 0;
    static constexpr int k_DirUp = 1;
    static constexpr int k_DirDown = -1;
};
