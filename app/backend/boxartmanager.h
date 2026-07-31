#pragma once

#include "computermanager.h"
#include <QDir>
#include <QImage>
#include <QHash>
#include <QMutex>
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

    static
    void
    deleteBoxArt(NvComputer* computer);

signals:
    void
    boxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

public slots:

private slots:
    void
    handleBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

private:
    QUrl
    loadBoxArtFromNetwork(NvComputer* computer, const NvApp& app);

    static bool
    isPlaceholderBoxArt(const QSize& size);

    bool
    isCachedPlaceholderBoxArt(const QString& cachePath);

    void
    rememberPlaceholderBoxArt(const QString& cachePath, bool isPlaceholder);

    QString
    getFilePathForBoxArt(NvComputer* computer, int appId);

    QDir m_BoxArtDir;
    QThreadPool m_ThreadPool;
    QMutex m_PlaceholderCacheMutex;
    QHash<QString, bool> m_PlaceholderCache;
};
