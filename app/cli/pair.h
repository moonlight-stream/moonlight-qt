#pragma once

#include "backend/nvapp.h"
#include <QObject>
#include <QVariant>
#include <QUrl>

class ComputerManager;
class NvComputer;
class Session;
class StreamingPreferences;

namespace CliPair
{

class Event;
class LauncherPrivate;

class Launcher : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(m_DPtr, Launcher)


public:
    explicit Launcher(QString computer, QString predefinedPin,
                      QObject *parent = nullptr);
    ~Launcher();
    Q_INVOKABLE void execute(ComputerManager *manager);
    Q_INVOKABLE bool isExecuted() const;

signals:
    void searchingComputer();
    void pairing(QString pcName, QString pin);
    void failed(QString text);
    void success();

private slots:
    void onComputerFound(NvComputer *computer);
    void onPairingCompleted(NvComputer *computer, QString error);
    void onTimeout();
    void onBoxArtLoadComplete(NvComputer* computer, NvApp app, QUrl image);

private:
    QScopedPointer<LauncherPrivate> m_DPtr;
};

}
