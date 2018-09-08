#pragma once

#include "computermanager.h"
#include <QDir>
#include <QImage>
#include <QThreadPool>
#include <QRunnable>

class BoxArtManager : public QObject
{
    Q_OBJECT

    friend class NetworkBoxArtLoadTask;

public:
    explicit BoxArtManager(QObject *parent = nullptr);

    QUrl
    loadBoxArt(NvComputer* computer, NvApp& app);

signals:
    void
    boxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

public slots:

private slots:
    void
    handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

private:
    QUrl
    loadBoxArtFromNetwork(NvComputer* computer, int appId);

    QString
    getFilePathForBoxArt(NvComputer* computer, int appId);

    QDir m_BoxArtDir;
    QThreadPool m_ThreadPool;
};

class NetworkBoxArtLoadTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    NetworkBoxArtLoadTask(BoxArtManager* boxArtManager, NvComputer* computer, NvApp& app)
        : m_Bam(boxArtManager),
          m_Computer(computer),
          m_App(app)
    {
        connect(this, SIGNAL(boxArtFetchCompleted(NvComputer*,NvApp,QUrl)),
                boxArtManager, SLOT(handleBoxArtLoadComplete(NvComputer*,NvApp,QUrl)));
    }

signals:
    void boxArtFetchCompleted(NvComputer* computer, NvApp app, QUrl image);

private:
    void run()
    {
        QUrl image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App.id);
        if (image.isEmpty()) {
            // Give it another shot if it fails once
            image = m_Bam->loadBoxArtFromNetwork(m_Computer, m_App.id);
        }
        emit boxArtFetchCompleted(m_Computer, m_App, image);
    }

    BoxArtManager* m_Bam;
    NvComputer* m_Computer;
    NvApp m_App;
};
