#pragma once

#include <QObject>
#include <QRect>

class SystemProperties : public QObject
{
    Q_OBJECT

public:
    SystemProperties();

    Q_PROPERTY(bool hasHardwareAcceleration MEMBER hasHardwareAcceleration CONSTANT)
    Q_PROPERTY(bool isRunningWayland MEMBER isRunningWayland CONSTANT)
    Q_PROPERTY(bool isWow64 MEMBER isWow64 CONSTANT)
    Q_PROPERTY(bool hasBrowser MEMBER hasBrowser CONSTANT)
    Q_PROPERTY(QString unmappedGamepads MEMBER unmappedGamepads NOTIFY unmappedGamepadsChanged)
    Q_PROPERTY(QString applicationStyle MEMBER applicationStyle CONSTANT)
    Q_PROPERTY(int maximumStreamingFrameRate MEMBER maximumStreamingFrameRate CONSTANT)


    Q_INVOKABLE QRect getDesktopResolution(int displayIndex);
    Q_INVOKABLE QRect getNativeResolution(int displayIndex);

signals:
    void unmappedGamepadsChanged();

private:
    void querySdlVideoInfo();

    bool hasHardwareAcceleration;
    bool isRunningWayland;
    bool isWow64;
    bool hasBrowser;
    QString unmappedGamepads;
    QString applicationStyle;
    int maximumStreamingFrameRate;
    QList<QRect> monitorDesktopResolutions;
    QList<QRect> monitorNativeResolutions;
};

