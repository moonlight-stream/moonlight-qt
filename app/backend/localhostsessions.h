#pragma once

#include <QObject>
#include <QVariantList>

class LocalHostSessions : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY statusChanged)
    Q_PROPERTY(bool available READ available NOTIFY statusChanged)
    Q_PROPERTY(quint64 since READ since NOTIFY statusChanged)
    Q_PROPERTY(quint64 now READ now NOTIFY clockChanged)
    Q_PROPERTY(bool disconnecting READ disconnecting NOTIFY disconnectingChanged)

public:
    using QObject::QObject;
    static bool isLocalAudioHost(const QString& id);
    QVariantList devices() const { return m_Devices; }
    bool available() const { return m_Available; }
    quint64 since() const { return m_Since; }
    quint64 now() const;
    bool disconnecting() const { return !m_Disconnect.isEmpty(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void disconnectDevice(const QString& visitId);
    Q_INVOKABLE void disconnectAll();

signals:
    void statusChanged();
    void clockChanged();
    void disconnectingChanged();
    void disconnectFailed();

private:
    void queueDisconnect(const QByteArray& command);
    void request(const QByteArray& command);

    QVariantList m_Devices;
    quint64 m_Since = 0;
    bool m_Available = false;
    bool m_InFlight = false;
    QByteArray m_Disconnect;
};
