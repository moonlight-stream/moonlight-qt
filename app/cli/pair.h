/**
 * @file app/cli/pair.h
 * @brief Command-line interface for pairing with GameStream servers.
 */

#pragma once

#include <QObject>
#include <QVariant>

class ComputerManager;
class NvComputer;
class Session;
class StreamingPreferences;

/**
 * @brief Command-line interface for pairing with GameStream servers.
 */
namespace CliPair
{

class Event;
class LauncherPrivate;

/**
 * @brief Launches the pairing process from the command line.
 */
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

private:
    QScopedPointer<LauncherPrivate> m_DPtr;
};

}
