#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QIcon>
#include <QQuickStyle>
#include <QMutex>
#include <QtDebug>

// Don't let SDL hook our main function, since Qt is already
// doing the same thing. This needs to be before any headers
// that might include SDL.h themselves.
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "gui/computermodel.h"
#include "gui/appmodel.h"
#include "streaming/session.hpp"
#include "settings/streamingpreferences.h"

#if !defined(QT_DEBUG) && defined(Q_OS_WIN32)
// Log to file for release Windows builds
#define USE_CUSTOM_LOGGER
#define LOG_TO_FILE
#elif defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
// Use stdout logger on all Linux/BSD builds
#define USE_CUSTOM_LOGGER
#elif !defined(QT_DEBUG) && defined(Q_OS_DARWIN)
// Log to file for release Mac builds
#define USE_CUSTOM_LOGGER
#define LOG_TO_FILE
#else
// For debug Windows and Mac builds, use default logger
#endif

#ifdef USE_CUSTOM_LOGGER
static QTime s_LoggerTime;
static QTextStream s_LoggerStream(stdout);
static QMutex s_LoggerLock;
#ifdef LOG_TO_FILE
static QFile* s_LoggerFile;
#endif

void sdlLogToDiskHandler(void*, int category, SDL_LogPriority priority, const char* message)
{
    QString priorityTxt;

    switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
        priorityTxt = "Verbose";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        priorityTxt = "Debug";
        break;
    case SDL_LOG_PRIORITY_INFO:
        priorityTxt = "Info";
        break;
    case SDL_LOG_PRIORITY_WARN:
        priorityTxt = "Warn";
        break;
    case SDL_LOG_PRIORITY_ERROR:
        priorityTxt = "Error";
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        priorityTxt = "Critical";
        break;
    }

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - SDL %2 (%3): %4").arg(logTime.toString()).arg(priorityTxt).arg(category).arg(message);

    {
        QMutexLocker lock(&s_LoggerLock);
        s_LoggerStream << txt << endl;
    }
}

void qtLogToDiskHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    QString typeTxt;

    switch (type) {
    case QtDebugMsg:
        typeTxt = "Debug";
        break;
    case QtInfoMsg:
        typeTxt = "Info";
        break;
    case QtWarningMsg:
        typeTxt = "Warning";
        break;
    case QtCriticalMsg:
        typeTxt = "Critical";
        break;
    case QtFatalMsg:
        typeTxt = "Fatal";
        break;
    }

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - Qt %2: %3").arg(logTime.toString()).arg(typeTxt).arg(msg);

    {
        QMutexLocker lock(&s_LoggerLock);
        s_LoggerStream << txt << endl;
    }
}

#endif

int main(int argc, char *argv[])
{
#ifdef USE_CUSTOM_LOGGER
#ifdef LOG_TO_FILE
    QDir tempDir(QDir::tempPath());
    s_LoggerFile = new QFile(tempDir.filePath(QString("Moonlight-%1.log").arg(QDateTime::currentSecsSinceEpoch())));
    if (s_LoggerFile->open(QIODevice::WriteOnly)) {
        qInfo() << "Redirecting log output to " << s_LoggerFile->fileName();
        s_LoggerStream.setDevice(s_LoggerFile);
    }
#endif

    s_LoggerTime.start();
    qInstallMessageHandler(qtLogToDiskHandler);
    SDL_LogSetOutputFunction(sdlLogToDiskHandler, nullptr);
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // This avoids using the default keychain for SSL, which may cause
    // password prompts on macOS.
    qputenv("QT_SSL_USE_TEMPORARY_KEYCHAIN", QByteArray("1"));

    // Set these here to allow us to use the default QSettings constructor
    QCoreApplication::setOrganizationName("Moonlight Game Streaming Project");
    QCoreApplication::setOrganizationDomain("moonlight-stream.com");
    QCoreApplication::setApplicationName("Moonlight");

    // Register custom metatypes for use in signals
    qRegisterMetaType<NvApp>("NvApp");

    QGuiApplication app(argc, argv);

    app.setWindowIcon(QIcon(":/res/moonlight.svg"));

    // Register our C++ types for QML
    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");
    qmlRegisterType<StreamingPreferences>("StreamingPreferences", 1, 0, "StreamingPreferences");
    qmlRegisterUncreatableType<Session>("Session", 1, 0, "Session", "Session cannot be created from QML");
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                  return new ComputerManager();
                                              });

    QQuickStyle::setStyle("Material");

    // Load the main.qml file
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    SDL_SetMainReady();
    if (SDL_Init(0) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Init() failed: %s",
                     SDL_GetError());
    }

    int err = app.exec();

    SDL_Quit();

    return err;
}
