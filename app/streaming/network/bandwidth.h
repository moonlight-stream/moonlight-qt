#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>

class BandwidthCalculator : public QObject
{
    Q_OBJECT

public:
    static BandwidthCalculator* instance();
    
    void addBytes(qint64 bytes);
    int getCurrentBandwidthKbps();
    
    void start();
    void stop();

private:
    BandwidthCalculator();
    ~BandwidthCalculator();
    
    static BandwidthCalculator *s_instance;
    
    std::atomic<qint64> m_bytesReceived;
    qint64 m_lastBytesReceived;
    std::atomic<int> m_currentBandwidthKbps;
    bool m_running;
    
    QTimer m_updateTimer;
    QElapsedTimer m_elapsedTimer;
    
private slots:
    void updateBandwidth();
};
