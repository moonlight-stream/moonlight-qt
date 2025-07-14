#include "servercommandmanager.h"
#include "nvcomputer.h"
#include <QJsonDocument>
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
        "sleep_system",
        "Sleep System", 
        "Put the host system to sleep",
        QJsonObject()
    },
    {
        "restart_system",
        "Restart System",
        "Restart the host system",
        QJsonObject()
    },
    {
        "shutdown_system",
        "Shutdown System", 
        "Shutdown the host system",
        QJsonObject()
    },
    {
        "lock_desktop",
        "Lock Desktop",
        "Lock the desktop session",
        QJsonObject()
    },
    {
        "unlock_desktop",
        "Unlock Desktop",
        "Unlock the desktop session", 
        QJsonObject{{"password", ""}}
    }
};

ServerCommandManager::ServerCommandManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_targetComputer(nullptr)
    , m_supported(false)
{
    qDebug() << "ServerCommandManager: Initialized";
}

ServerCommandManager::~ServerCommandManager()
{
    qDebug() << "ServerCommandManager: Destroyed";
}

void ServerCommandManager::setTargetComputer(NvComputer *computer)
{
    m_targetComputer = computer;
    
    if (computer) {
        qDebug() << "ServerCommandManager: Set target computer:" << computer->name;
        
        // Check if server supports commands
        refreshCommands();
    } else {
        qDebug() << "ServerCommandManager: Cleared target computer";
        m_supported = false;
        m_availableCommands = QJsonArray();
        emit supportStatusChanged(false);
        emit commandsUpdated(QJsonArray());
    }
}

bool ServerCommandManager::isServerCommandsSupported() const
{
    return m_supported;
}

QJsonArray ServerCommandManager::getAvailableCommands() const
{
    return m_availableCommands;
}

void ServerCommandManager::executeCommand(const QString &commandId, const QJsonObject &parameters)
{
    if (!m_targetComputer || !m_supported) {
        qWarning() << "ServerCommandManager: Cannot execute command - no target or not supported";
        emit commandExecuted(commandId, false, QJsonObject{{"error", "Server commands not available"}});
        return;
    }
    
    qDebug() << "ServerCommandManager: Executing command:" << commandId;
    
    QJsonObject requestData;
    requestData["command"] = commandId;
    requestData["parameters"] = parameters;
    
    QNetworkReply *reply = sendServerRequest("/api/commands/execute", requestData);
    if (reply) {
        // Store command ID in reply for later reference
        reply->setProperty("commandId", commandId);
        connect(reply, &QNetworkReply::finished, 
                this, &ServerCommandManager::onCommandExecutionReceived);
    } else {
        emit commandExecuted(commandId, false, QJsonObject{{"error", "Failed to send request"}});
    }
}

void ServerCommandManager::refreshCommands()
{
    if (!m_targetComputer) {
        return;
    }
    
    qDebug() << "ServerCommandManager: Refreshing commands list";
    
    QNetworkReply *reply = sendServerRequest("/api/commands/list");
    if (reply) {
        connect(reply, &QNetworkReply::finished,
                this, &ServerCommandManager::onCommandsListReceived);
    }
}

void ServerCommandManager::onCommandsListReceived()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    reply->deleteLater();
    
    QJsonObject result;
    bool success = parseServerResponse(reply, result);
    
    if (success && result.contains("commands")) {
        m_supported = true;
        m_availableCommands = result["commands"].toArray();
        
        qDebug() << "ServerCommandManager: Received" << m_availableCommands.size() << "commands from server";
        
        emit supportStatusChanged(true);
        emit commandsUpdated(m_availableCommands);
    } else {
        // Server doesn't support commands API, fall back to builtin commands
        qDebug() << "ServerCommandManager: Server doesn't support commands API, using builtin commands";
        
        m_supported = false;  // Mark as not supported for now
        m_availableCommands = QJsonArray();
        
        // Convert builtin commands to JSON array
        QJsonArray builtinArray;
        for (const auto &cmd : BUILTIN_COMMANDS) {
            QJsonObject cmdObj;
            cmdObj["id"] = cmd.id;
            cmdObj["name"] = cmd.name;
            cmdObj["description"] = cmd.description;
            cmdObj["parameters"] = cmd.parameters;
            builtinArray.append(cmdObj);
        }
        
        m_availableCommands = builtinArray;
        
        emit supportStatusChanged(false);
        emit commandsUpdated(m_availableCommands);
    }
}

void ServerCommandManager::onCommandExecutionReceived()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    QString commandId = reply->property("commandId").toString();
    reply->deleteLater();
    
    QJsonObject result;
    bool success = parseServerResponse(reply, result);
    
    if (success) {
        qDebug() << "ServerCommandManager: Command" << commandId << "executed successfully";
    } else {
        qWarning() << "ServerCommandManager: Command" << commandId << "failed";
    }
    
    emit commandExecuted(commandId, success, result);
}

QNetworkReply* ServerCommandManager::sendServerRequest(const QString &endpoint, const QJsonObject &data)
{
    if (!m_targetComputer) {
        return nullptr;
    }
    
    QString url = getServerBaseUrl() + endpoint;
    QNetworkRequest request(url);
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "Artemis-Qt/1.0");
    
    // Add authentication if needed (this would need to be implemented based on server requirements)
    // request.setRawHeader("Authorization", "Bearer " + token);
    
    QNetworkReply *reply;
    if (data.isEmpty()) {
        reply = m_networkManager->get(request);
    } else {
        QJsonDocument doc(data);
        reply = m_networkManager->post(request, doc.toJson());
    }
    
    // Set timeout
    QTimer::singleShot(10000, reply, &QNetworkReply::abort);  // 10 second timeout
    
    return reply;
}

bool ServerCommandManager::parseServerResponse(QNetworkReply *reply, QJsonObject &result)
{
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "ServerCommandManager: Network error:" << reply->errorString();
        result["error"] = reply->errorString();
        return false;
    }
    
    QByteArray responseData = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ServerCommandManager: JSON parse error:" << parseError.errorString();
        result["error"] = "Invalid JSON response";
        return false;
    }
    
    result = doc.object();
    
    // Check for server-side errors
    if (result.contains("error")) {
        qWarning() << "ServerCommandManager: Server error:" << result["error"].toString();
        return false;
    }
    
    return true;
}

QString ServerCommandManager::getServerBaseUrl() const
{
    if (!m_targetComputer) {
        return QString();
    }
    
    // Build base URL for Apollo/Sunshine server
    // This assumes the server has an HTTP API on the same host
    QString protocol = "https";  // Apollo typically uses HTTPS
    QString host = m_targetComputer->activeAddress;
    int port = 47990;  // Default Apollo HTTP port
    
    return QString("%1://%2:%3").arg(protocol, host, QString::number(port));
}