#include "nvhttp.h"
#include <Limelight.h>

#include <QDebug>
#include <QUuid>
#include <QtNetwork/QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QXmlStreamReader>
#include <QSslKey>
#include <QImageReader>
#include <QtEndian>
#include <QNetworkProxy>

#define FAST_FAIL_TIMEOUT_MS 2000
#define REQUEST_TIMEOUT_MS 5000
#define LAUNCH_TIMEOUT_MS 120000
#define RESUME_TIMEOUT_MS 30000
#define QUIT_TIMEOUT_MS 30000

NvHTTP::NvHTTP(QString address, QSslCertificate serverCert) :
    m_ServerCert(serverCert)
{
    m_BaseUrlHttp.setScheme("http");
    m_BaseUrlHttps.setScheme("https");
    m_BaseUrlHttp.setPort(47989);
    m_BaseUrlHttps.setPort(47984);

    setAddress(address);

    // Never use a proxy server
    QNetworkProxy noProxy(QNetworkProxy::NoProxy);
    m_Nam.setProxy(noProxy);

    connect(&m_Nam, &QNetworkAccessManager::sslErrors, this, &NvHTTP::handleSslErrors);
}

void NvHTTP::setServerCert(QSslCertificate serverCert)
{
    m_ServerCert = serverCert;
}

void NvHTTP::setAddress(QString address)
{
    Q_ASSERT(!address.isEmpty());

    m_Address = address;

    m_BaseUrlHttp.setHost(address);
    m_BaseUrlHttps.setHost(address);
}

QString NvHTTP::address()
{
    return m_Address;
}

QVector<int>
NvHTTP::parseQuad(QString quad)
{
    QVector<int> ret;

    // Return an empty vector for old GFE versions
    // that were missing GfeVersion.
    if (quad.isEmpty()) {
        return ret;
    }

    QStringList parts = quad.split(".");
    for (int i = 0; i < 4; i++)
    {
        ret.append(parts.at(i).toInt());
    }

    return ret;
}

int
NvHTTP::getCurrentGame(QString serverInfo)
{
    // GFE 2.8 started keeping currentgame set to the last game played. As a result, it no longer
    // has the semantics that its name would indicate. To contain the effects of this change as much
    // as possible, we'll force the current game to zero if the server isn't in a streaming session.
    QString serverState = getXmlString(serverInfo, "state");
    if (serverState != nullptr && serverState.endsWith("_SERVER_BUSY"))
    {
        return getXmlString(serverInfo, "currentgame").toInt();
    }
    else
    {
        return 0;
    }
}

QString
NvHTTP::getServerInfo(NvLogLevel logLevel, bool fastFail)
{
    QString serverInfo;

    // Check if we have a pinned cert for this host yet
    if (!m_ServerCert.isNull())
    {
        try
        {
            // Always try HTTPS first, since it properly reports
            // pairing status (and a few other attributes).
            serverInfo = openConnectionToString(m_BaseUrlHttps,
                                                "serverinfo",
                                                nullptr,
                                                fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                                logLevel);
            // Throws if the request failed
            verifyResponseStatus(serverInfo);
        }
        catch (const GfeHttpResponseException& e)
        {
            if (e.getStatusCode() == 401)
            {
                // Certificate validation error, fallback to HTTP
                serverInfo = openConnectionToString(m_BaseUrlHttp,
                                                    "serverinfo",
                                                    nullptr,
                                                    fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                                    logLevel);
                verifyResponseStatus(serverInfo);
            }
            else
            {
                // Rethrow real errors
                throw e;
            }
        }
    }
    else
    {
        // Only use HTTP prior to pairing
        serverInfo = openConnectionToString(m_BaseUrlHttp,
                                            "serverinfo",
                                            nullptr,
                                            fastFail ? FAST_FAIL_TIMEOUT_MS : REQUEST_TIMEOUT_MS,
                                            logLevel);
        verifyResponseStatus(serverInfo);
    }

    return serverInfo;
}

void
NvHTTP::launchApp(int appId,
                  PSTREAM_CONFIGURATION streamConfig,
                  bool sops,
                  bool localAudio,
                  int gamepadMask)
{
    int riKeyId;

    memcpy(&riKeyId, streamConfig->remoteInputAesIv, sizeof(riKeyId));
    riKeyId = qFromBigEndian(riKeyId);

    QString response =
            openConnectionToString(m_BaseUrlHttps,
                                   "launch",
                                   "appid="+QString::number(appId)+
                                   "&mode="+QString::number(streamConfig->width)+"x"+
                                   QString::number(streamConfig->height)+"x"+
                                   // Using an FPS value over 60 causes SOPS to default to 720p60,
                                   // so force it to 0 to ensure the correct resolution is set. We
                                   // used to use 60 here but that locked the frame rate to 60 FPS
                                   // on GFE 3.20.3.
                                   QString::number(streamConfig->fps > 60 ? 0 : streamConfig->fps)+
                                   "&additionalStates=1&sops="+QString::number(sops ? 1 : 0)+
                                   "&rikey="+QByteArray(streamConfig->remoteInputAesKey, sizeof(streamConfig->remoteInputAesKey)).toHex()+
                                   "&rikeyid="+QString::number(riKeyId)+
                                   (streamConfig->enableHdr ?
                                       "&hdrMode=1&clientHdrCapVersion=0&clientHdrCapSupportedFlagsInUint32=0&clientHdrCapMetaDataId=NV_STATIC_METADATA_TYPE_1&clientHdrCapDisplayData=0x0x0x0x0x0x0x0x0x0x0" :
                                        "")+
                                   "&localAudioPlayMode="+QString::number(localAudio ? 1 : 0)+
                                   "&surroundAudioInfo="+QString::number(SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(streamConfig->audioConfiguration))+
                                   "&remoteControllersBitmap="+QString::number(gamepadMask)+
                                   "&gcmap="+QString::number(gamepadMask),
                                   LAUNCH_TIMEOUT_MS);

    // Throws if the request failed
    verifyResponseStatus(response);
}

void
NvHTTP::resumeApp(PSTREAM_CONFIGURATION streamConfig)
{
    int riKeyId;

    memcpy(&riKeyId, streamConfig->remoteInputAesIv, sizeof(riKeyId));
    riKeyId = qFromBigEndian(riKeyId);

    QString response =
            openConnectionToString(m_BaseUrlHttps,
                                   "resume",
                                   "rikey="+QString(QByteArray(streamConfig->remoteInputAesKey, sizeof(streamConfig->remoteInputAesKey)).toHex())+
                                   "&rikeyid="+QString::number(riKeyId)+
                                   "&surroundAudioInfo="+QString::number(SURROUNDAUDIOINFO_FROM_AUDIO_CONFIGURATION(streamConfig->audioConfiguration)),
                                   RESUME_TIMEOUT_MS);

    // Throws if the request failed
    verifyResponseStatus(response);
}

void
NvHTTP::quitApp()
{
    QString response =
            openConnectionToString(m_BaseUrlHttps,
                                   "cancel",
                                   nullptr,
                                   QUIT_TIMEOUT_MS);

    // Throws if the request failed
    verifyResponseStatus(response);

    // Newer GFE versions will just return success even if quitting fails
    // if we're not the original requestor.
    if (getCurrentGame(getServerInfo(NvHTTP::NVLL_ERROR)) != 0) {
        // Generate a synthetic GfeResponseException letting the caller know
        // that they can't kill someone else's stream.
        throw GfeHttpResponseException(599, "");
    }
}

QVector<NvDisplayMode>
NvHTTP::getDisplayModeList(QString serverInfo)
{
    QXmlStreamReader xmlReader(serverInfo);
    QVector<NvDisplayMode> modes;

    while (!xmlReader.atEnd()) {
        while (xmlReader.readNextStartElement()) {
            auto name = xmlReader.name();
            if (name == QString("DisplayMode")) {
                modes.append(NvDisplayMode());
            }
            else if (name == QString("Width")) {
                modes.last().width = xmlReader.readElementText().toInt();
            }
            else if (name == QString("Height")) {
                modes.last().height = xmlReader.readElementText().toInt();
            }
            else if (name == QString("RefreshRate")) {
                modes.last().refreshRate = xmlReader.readElementText().toInt();
            }
        }
    }

    return modes;
}

QVector<NvApp>
NvHTTP::getAppList()
{
    QString appxml = openConnectionToString(m_BaseUrlHttps,
                                            "applist",
                                            nullptr,
                                            REQUEST_TIMEOUT_MS,
                                            NvLogLevel::NVLL_ERROR);
    verifyResponseStatus(appxml);

    QXmlStreamReader xmlReader(appxml);
    QVector<NvApp> apps;
    while (!xmlReader.atEnd()) {
        while (xmlReader.readNextStartElement()) {
            auto name = xmlReader.name();
            if (name == QString("App")) {
                // We must have a valid app before advancing to the next one
                if (!apps.isEmpty() && !apps.last().isInitialized()) {
                    qWarning() << "Invalid applist XML";
                    Q_ASSERT(false);
                    return QVector<NvApp>();
                }
                apps.append(NvApp());
            }
            else if (name == QString("AppTitle")) {
                apps.last().name = xmlReader.readElementText();
            }
            else if (name == QString("ID")) {
                apps.last().id = xmlReader.readElementText().toInt();
            }
            else if (name == QString("IsHdrSupported")) {
                apps.last().hdrSupported = xmlReader.readElementText() == "1";
            }
            else if (name == QString("IsAppCollectorGame")) {
                apps.last().isAppCollectorGame = xmlReader.readElementText() == "1";
            }
        }
    }

    return apps;
}

void
NvHTTP::verifyResponseStatus(QString xml)
{
    QXmlStreamReader xmlReader(xml);

    while (xmlReader.readNextStartElement())
    {
        if (xmlReader.name() == QString("root"))
        {
            // Status code can be 0xFFFFFFFF in some rare cases on GFE 3.20.3, and
            // QString::toInt() will fail in that case, so use QString::toUInt()
            // and cast the result to an int instead.
            int statusCode = (int)xmlReader.attributes().value("status_code").toUInt();
            if (statusCode == 200)
            {
                // Successful
                return;
            }
            else
            {
                QString statusMessage = xmlReader.attributes().value("status_message").toString();
                if (statusCode != 401) {
                    // 401 is expected for unpaired PCs when we fetch serverinfo over HTTPS
                    qWarning() << "Request failed:" << statusCode << statusMessage;
                }
                if (statusCode == -1 && statusMessage == "Invalid") {
                    // Special case handling an audio capture error which GFE doesn't
                    // provide any useful status message for.
                    statusCode = 418;
                    statusMessage = tr("Missing audio capture device. Reinstalling GeForce Experience should resolve this error.");
                }
                throw GfeHttpResponseException(statusCode, statusMessage);
            }
        }
    }
}

QImage
NvHTTP::getBoxArt(int appId)
{
    QNetworkReply* reply = openConnection(m_BaseUrlHttps,
                                          "appasset",
                                          "appid="+QString::number(appId)+
                                          "&AssetType=2&AssetIdx=0",
                                          REQUEST_TIMEOUT_MS,
                                          NvLogLevel::NVLL_VERBOSE);
    QImage image = QImageReader(reply).read();
    delete reply;

    return image;
}

QByteArray
NvHTTP::getXmlStringFromHex(QString xml,
                            QString tagName)
{
    QString str = getXmlString(xml, tagName);
    if (str == nullptr)
    {
        return nullptr;
    }

    return QByteArray::fromHex(str.toLatin1());
}

QString
NvHTTP::getXmlString(QString xml,
                     QString tagName)
{
    QXmlStreamReader xmlReader(xml);

    while (!xmlReader.atEnd())
    {
        if (xmlReader.readNext() != QXmlStreamReader::StartElement)
        {
            continue;
        }

        if (xmlReader.name() == tagName)
        {
            return xmlReader.readElementText();
        }
    }

    return nullptr;
}

void NvHTTP::handleSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    bool ignoreErrors = true;

    if (m_ServerCert.isNull()) {
        // We should never make an HTTPS request without a cert
        Q_ASSERT(!m_ServerCert.isNull());
        return;
    }

    for (auto error : errors) {
        if (m_ServerCert != error.certificate()) {
            ignoreErrors = false;
            break;
        }
    }

    if (ignoreErrors) {
        reply->ignoreSslErrors(errors);
    }
}

QString
NvHTTP::openConnectionToString(QUrl baseUrl,
                               QString command,
                               QString arguments,
                               int timeoutMs,
                               NvLogLevel logLevel)
{
    QNetworkReply* reply = openConnection(baseUrl, command, arguments, timeoutMs, logLevel);
    QString ret;

    QTextStream stream(reply);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setEncoding(QStringConverter::Utf8);
#else
    stream.setCodec("UTF-8");
#endif

    ret = stream.readAll();
    delete reply;

    return ret;
}

QNetworkReply*
NvHTTP::openConnection(QUrl baseUrl,
                       QString command,
                       QString arguments,
                       int timeoutMs,
                       NvLogLevel logLevel)
{
    // Build a URL for the request
    QUrl url(baseUrl);
    url.setPath("/" + command);

    // Use a common UID for Moonlight clients to allow them to quit
    // games for each other (otherwise GFE gets screwed up and it requires
    // manual intervention to solve).
    url.setQuery("uniqueid=0123456789ABCDEF&uuid=" +
                 QUuid::createUuid().toRfc4122().toHex() +
                 ((arguments != nullptr) ? ("&" + arguments) : ""));

    QNetworkRequest request(url);

    // Add our client certificate
    request.setSslConfiguration(IdentityManager::get()->getSslConfig());

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(5, 15, 1) && !defined(QT_NO_BEARERMANAGEMENT)
    // HACK: Set network accessibility to work around QTBUG-80947 (introduced in Qt 5.14.0 and fixed in Qt 5.15.1)
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    m_Nam.setNetworkAccessible(QNetworkAccessManager::Accessible);
    QT_WARNING_POP
#endif

    QNetworkReply* reply = m_Nam.get(request);

    // Run the request with a timeout if requested
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), &loop, SLOT(quit()));
    if (timeoutMs) {
        QTimer::singleShot(timeoutMs, &loop, SLOT(quit()));
    }
    if (logLevel >= NvLogLevel::NVLL_VERBOSE) {
        qInfo() << "Executing request:" << url.toString();
    }
    loop.exec(QEventLoop::ExcludeUserInputEvents);

    // Abort the request if it timed out
    if (!reply->isFinished())
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << "Aborting timed out request for" << url.toString();
        }
        reply->abort();
    }

    // We must clear out cached authentication and connections or
    // GFE will puke next time
    m_Nam.clearAccessCache();

    // Handle error
    if (reply->error() != QNetworkReply::NoError)
    {
        if (logLevel >= NvLogLevel::NVLL_ERROR) {
            qWarning() << command << " request failed with error " << reply->error();
        }

        if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
            // This will trigger falling back to HTTP for the serverinfo query
            // then pairing again to get the updated certificate.
            GfeHttpResponseException exception(401, "Server certificate mismatch");
            delete reply;
            throw exception;
        }
        else if (reply->error() == QNetworkReply::OperationCanceledError) {
            QtNetworkReplyException exception(QNetworkReply::TimeoutError, "Request timed out");
            delete reply;
            throw exception;
        }
        else {
            QtNetworkReplyException exception(reply->error(), reply->errorString());
            delete reply;
            throw exception;
        }
    }

    return reply;
}
