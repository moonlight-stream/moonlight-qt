#pragma once

#include <QObject>
#include <QRect>

class SystemProperties : public QObject
{
    Q_OBJECT

    friend class QuerySdlVideoThread;
    friend class RefreshDisplaysThread;

public:
    SystemProperties();

    Q_PROPERTY(bool hasHardwareAcceleration MEMBER hasHardwareAcceleration CONSTANT)
    Q_PROPERTY(bool rendererAlwaysFullScreen MEMBER rendererAlwaysFullScreen CONSTANT)
    Q_PROPERTY(bool isRunningWayland MEMBER isRunningWayland CONSTANT)
    Q_PROPERTY(bool isRunningXWayland MEMBER isRunningXWayland CONSTANT)
    Q_PROPERTY(bool isWow64 MEMBER isWow64 CONSTANT)
    Q_PROPERTY(QString friendlyNativeArchName MEMBER friendlyNativeArchName CONSTANT)
    Q_PROPERTY(bool hasDesktopEnvironment MEMBER hasDesktopEnvironment CONSTANT)
    Q_PROPERTY(bool hasBrowser MEMBER hasBrowser CONSTANT)
    Q_PROPERTY(bool hasDiscordIntegration MEMBER hasDiscordIntegration CONSTANT)
    Q_PROPERTY(QString unmappedGamepads MEMBER unmappedGamepads NOTIFY unmappedGamepadsChanged)
    Q_PROPERTY(int maximumStreamingFrameRate MEMBER maximumStreamingFrameRate CONSTANT)
    Q_PROPERTY(QSize maximumResolution MEMBER maximumResolution CONSTANT)
    Q_PROPERTY(QString versionString MEMBER versionString CONSTANT)
    Q_PROPERTY(bool supportsHdr MEMBER supportsHdr CONSTANT)

    Q_INVOKABLE void refreshDisplays();
    Q_INVOKABLE QRect getDesktopResolution(int displayIndex);
    Q_INVOKABLE QRect getNativeResolution(int displayIndex);

signals:
    void unmappedGamepadsChanged();

private:
    void querySdlVideoInfo();
    void querySdlVideoInfoInternal();
    void refreshDisplaysInternal();

    bool hasHardwareAcceleration;
    bool rendererAlwaysFullScreen;
    bool isRunningWayland;
    bool isRunningXWayland;
    bool isWow64;
    QString friendlyNativeArchName;
    bool hasDesktopEnvironment;
    bool hasBrowser;
    bool hasDiscordIntegration;
    QString unmappedGamepads;
    int maximumStreamingFrameRate;
    QSize maximumResolution;
    QList<QRect> monitorDesktopResolutions;
    QList<QRect> monitorNativeResolutions;
    QString versionString;
    bool supportsHdr;
};

