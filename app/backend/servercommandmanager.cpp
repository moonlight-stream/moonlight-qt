#include "servercommandmanager.h"
#include "nvcomputer.h"
#include "nvhttp.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDebug>

// Define builtin commands that Artemis supports
const QList<ServerCommandManager::ServerCommand> ServerCommandManager::BUILTIN_COMMANDS = {
    {
        "restart_server",
        "Restart Server",
        "Restart the Apollo/Sunshine server",
        QJsonObject()
    },
    {
        "shutdown_server", 
        "Shutdown Server",
        "Shutdown the Apollo/Sunshine server",
        QJsonObject()
    },
    {
        "restart_computer",
        "Restart Computer", 
        "Restart the host computer",
        QJsonObject()
    },
    {
        "shutdown_computer",
        "Shutdown Computer",
        "Shutdown the host computer", 
        QJsonObject()
    },
    {
        "suspend_computer",
        "Suspend Computer",
        "Suspend the host computer",
        QJsonObject()
    },
    {
        "hibernate_computer", 
        "Hibernate Computer",
        "Hibernate the host computer",
        QJsonObject()
    }
};

ServerCommandManager::ServerCommandManager(QObject *parent)
    : QObject(parent)
    , m_computer(nullptr)
    , m_http(nullptr)
    , m_hasPermission(false)
    , m_refreshInProgress(false)
{
    qDebug() << "ServerCommandManager: Initialized";
}

ServerCommandManager::~ServerCommandManager()
{
    qDebug() << "ServerCommandManager: Destroyed";
}

void ServerCommandManager::setConnection(NvComputer *computer, NvHTTP *http)
{
    m_computer = computer;
    m_http = http;
    
    // Check if this is an Apollo server and refresh commands
    if (computer && http) {
        refreshCommands();
    } else {
        m_hasPermission = false;
        m_availableCommands.clear();
        m_commandNames.clear();
        m_commandDescriptions.clear();
        emit commandsRefreshed();
    }
}

void ServerCommandManager::disconnect()
{
    m_computer = nullptr;
    m_http = nullptr;
    m_hasPermission = false;
    m_availableCommands.clear();
    m_commandNames.clear();
    m_commandDescriptions.clear();
    emit commandsRefreshed();
}

bool ServerCommandManager::hasServerCommandPermission() const
{
    return m_hasPermission;
}

void ServerCommandManager::refreshCommands()
{
    if (m_refreshInProgress || !m_computer || !m_http) {
        return;
    }
    
    m_refreshInProgress = true;
    
    // For now, just populate with builtin commands if it's an Apollo server
    if (isApolloServer()) {
        m_hasPermission = true;
        m_availableCommands.clear();
        m_commandNames.clear();
        m_commandDescriptions.clear();
        
        for (const auto &cmd : BUILTIN_COMMANDS) {
            m_availableCommands.append(cmd.id);
            m_commandNames[cmd.id] = cmd.name;
            m_commandDescriptions[cmd.id] = cmd.description;
        }
    } else {
        m_hasPermission = false;
        m_availableCommands.clear();
        m_commandNames.clear();
        m_commandDescriptions.clear();
    }
    
    m_refreshInProgress = false;
    emit commandsRefreshed();
}

void ServerCommandManager::executeCommand(const QString &commandId)
{
    if (!m_computer || !m_http || !m_hasPermission) {
        emit commandExecuted(commandId, false, "Server commands not available");
        return;
    }
    
    if (!m_availableCommands.contains(commandId)) {
        emit commandExecuted(commandId, false, "Command not found");
        return;
    }
    
    qDebug() << "ServerCommandManager: Executing command:" << commandId;
    
    // TODO: Implement actual command execution via HTTP
    // For now, just simulate success
    emit commandExecuted(commandId, true, "Command executed successfully");
}

QStringList ServerCommandManager::getAvailableCommands() const
{
    return m_availableCommands;
}

QString ServerCommandManager::getCommandName(const QString &commandId) const
{
    return m_commandNames.value(commandId, commandId);
}

QString ServerCommandManager::getCommandDescription(const QString &commandId) const
{
    return m_commandDescriptions.value(commandId, "No description available");
}

bool ServerCommandManager::isApolloServer() const
{
    if (!m_computer) {
        return false;
    }
    
    // Check if this is an Apollo/Sunshine server based on available properties
    // Apollo/Sunshine servers are typically non-NVIDIA software
    if (!m_computer->isNvidiaServerSoftware) {
        return true;
    }
    
    // Also check app version or GFE version for Apollo/Sunshine indicators
    QString appVer = m_computer->appVersion;
    QString gfeVer = m_computer->gfeVersion;
    
    return appVer.contains("Apollo", Qt::CaseInsensitive) || 
           appVer.contains("Sunshine", Qt::CaseInsensitive) ||
           gfeVer.contains("Apollo", Qt::CaseInsensitive) ||
           gfeVer.contains("Sunshine", Qt::CaseInsensitive);
}

void ServerCommandManager::fetchAvailableCommands()
{
    // TODO: Implement HTTP request to fetch commands from server
    // This would make a request to /api/commands or similar endpoint
}

void ServerCommandManager::sendCommandExecution(const QString &commandId)
{
    Q_UNUSED(commandId)
    // TODO: Implement HTTP request to execute command on server
    // This would make a POST request to /api/commands/{commandId}/execute
}

void ServerCommandManager::onCommandsReceived()
{
    // TODO: Handle response from fetchAvailableCommands()
    // This would parse the HTTP response and update available commands
    qDebug() << "ServerCommandManager: Commands received from server";
}

void ServerCommandManager::onCommandExecutionFinished()
{
    // TODO: Handle response from sendCommandExecution()
    // This would parse the execution result and emit commandExecuted signal
    qDebug() << "ServerCommandManager: Command execution finished";
}