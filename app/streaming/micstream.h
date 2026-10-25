#pragma once

#include <QObject>
class QThread;
class MicStreamWorker;

class MicStream : public QObject
{
    Q_OBJECT

public:
    explicit MicStream(QObject *parent = nullptr);
    ~MicStream();

    bool start();
    void stop();

private:
    QThread *m_thread;
    MicStreamWorker *m_worker;
};
