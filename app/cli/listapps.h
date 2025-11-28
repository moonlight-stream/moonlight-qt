/**
 * @file app/cli/listapps.h
 * @brief Command-line interface for listing available GameStream applications.
 */

#pragma once

#include "commandlineparser.h"

#include <QObject>
#include <QVariant>

class ComputerManager;
class NvComputer;

/**
 * @brief Command-line interface for listing GameStream applications.
 */
namespace CliListApps
{

class LauncherPrivate;

/**
 * @brief Launches the list apps command from the command line.
 */
class Launcher : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(m_DPtr, Launcher)

public:
    explicit Launcher(QString computer, ListCommandLineParser arguments, QObject *parent = nullptr);
    ~Launcher();

    Q_INVOKABLE void execute(ComputerManager *manager);
    Q_INVOKABLE bool isExecuted() const;

private slots:
    void onComputerFound(NvComputer *computer);
    void onComputerSeekTimeout();

private:
    QScopedPointer<LauncherPrivate> m_DPtr;
    ListCommandLineParser m_Arguments;
};

}
