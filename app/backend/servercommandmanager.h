#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QAbstractListModel>

class NvComputer;
class NvHTTP;

/**
 * @brief Manages server commands functionality with Apollo servers
 * 
 * This class implements server commands as used in Artemis Android.
 * It requires the server_cmd permission from Apollo servers.
 * Based on GameMenu.java implementation.
 */
class ServerCommandManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ServerCommandManager(QObject *parent = nullptr);
    ~ServerCommandManager();

    // Connection management
    Q_INVOKABLE void setConnection(NvComputer *computer, NvHTTP *http);
    Q_INVOKABLE void disconnect();

    // Command operations (matches Android API)
    Q_INVOKABLE bool hasServerCommandPermission() const;
    Q_INVOKABLE void refreshCommands();
    Q_INVOKABLE void executeCommand(const QString &commandId);

    // Command list access
    Q_INVOKABLE QStringList getAvailableCommands() const;
    Q_INVOKABLE QString getCommandName(const QString &commandId) const;
    Q_INVOKABLE QString getCommandDescription(const QString &commandId) const;

    // Apollo server detection (matches Android logic)
    Q_INVOKABLE bool isApolloServer() const;

signals:
    void commandsRefreshed();
    void commandExecuted(const QString &commandId, bool success, const QString &result);
    void commandExecutionFailed(const QString &commandId, const QString &error);
    void permissionChanged(bool hasPermission);
    void noCommandsAvailable(); // Matches Android dialog

private slots:
    void onCommandsReceived();
    void onCommandExecutionFinished();

private:
    // Permission checking (matches Android ComputerDetails.Operations)
    bool checkServerCommandPermission();
    
    // HTTP operations (placeholder for actual implementation)
    void fetchAvailableCommands();
    void sendCommandExecution(const QString &commandId);

private:
    NvComputer *m_computer;
    NvHTTP *m_http;
    
    bool m_hasPermission;
    QStringList m_availableCommands;
    QHash<QString, QString> m_commandNames;
    QHash<QString, QString> m_commandDescriptions;
    
    bool m_refreshInProgress;
    QString m_currentExecutingCommand;
};