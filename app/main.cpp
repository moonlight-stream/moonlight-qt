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

#include "path.h"
#include "gui/computermodel.h"
#include "gui/appmodel.h"
#include "backend/autoupdatechecker.h"
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
#define MAX_LOG_LINES 10000
static int s_LogLinesWritten = 0;
static bool s_LogLimitReached = false;
static QFile* s_LoggerFile;
#endif

void logToLoggerStream(QString& message)
{
    QMutexLocker lock(&s_LoggerLock);

#ifdef LOG_TO_FILE
    if (s_LogLimitReached) {
        return;
    }
    else if (s_LogLinesWritten == MAX_LOG_LINES) {
        s_LoggerStream << "Log size limit reached!" << endl;
        s_LogLimitReached = true;
        return;
    }
    else {
        s_LogLinesWritten++;
    }
#endif

    s_LoggerStream << message << endl;
}

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
    default:
        priorityTxt = "Unknown";
        break;
    }

    QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
    QString txt = QString("%1 - SDL %2 (%3): %4").arg(logTime.toString()).arg(priorityTxt).arg(category).arg(message);

    logToLoggerStream(txt);
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

    logToLoggerStream(txt);
}

#endif

#ifdef Q_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <DbgHelp.h>

static UINT s_HitUnhandledException = 0;

LONG WINAPI UnhandledExceptionHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{
    // Only write a dump for the first unhandled exception
    if (InterlockedCompareExchange(&s_HitUnhandledException, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    WCHAR dmpFileName[MAX_PATH];
    swprintf_s(dmpFileName, L"%ls\\Moonlight-%I64u.dmp",
               (PWCHAR)QDir::toNativeSeparators(Path::getLogDir()).utf16(), QDateTime::currentSecsSinceEpoch());
    QString qDmpFileName = QString::fromUtf16((unsigned short*)dmpFileName);
    HANDLE dumpHandle = CreateFileW(dmpFileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpHandle != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION info;

        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = ExceptionInfo;
        info.ClientPointers = FALSE;

        DWORD typeFlags = MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpIgnoreInaccessibleMemory |
                MiniDumpWithUnloadedModules |
                MiniDumpWithThreadInfo;

        if (MiniDumpWriteDump(GetCurrentProcess(),
                               GetCurrentProcessId(),
                               dumpHandle,
                               (MINIDUMP_TYPE)typeFlags,
                               &info,
                               nullptr,
                               nullptr)) {
            qCritical() << "Unhandled exception! Minidump written to:" << qDmpFileName;
        }
        else {
            qCritical() << "Unhandled exception! Failed to write dump:" << GetLastError();
        }

        CloseHandle(dumpHandle);
    }
    else {
        qCritical() << "Unhandled exception! Failed to open dump file:" << qDmpFileName << "with error" << GetLastError();
    }

    // Let the program crash and WER collect a dump
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

int main(int argc, char *argv[])
{
    // Set these here to allow us to use the default QSettings constructor.
    // These also ensure that our cache directory is named correctly. As such,
    // it is critical that these be called before Path::initialize().
    QCoreApplication::setOrganizationName("Moonlight Game Streaming Project");
    QCoreApplication::setOrganizationDomain("moonlight-stream.com");
    QCoreApplication::setApplicationName("Moonlight");

    if (QFile(QDir::currentPath() + "/portable.dat").exists()) {
        qInfo() << "Running in portable mode from:" << QDir::currentPath();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::currentPath());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, QDir::currentPath());

        // Initialize paths for portable mode
        Path::initialize(true);
    }
    else {
        // Initialize paths for standard installation
        Path::initialize(false);
    }

#ifdef USE_CUSTOM_LOGGER
#ifdef LOG_TO_FILE
    QDir tempDir(Path::getLogDir());
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

#ifdef Q_OS_WIN32
    // Create a crash dump when we crash on Windows
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // This avoids using the default keychain for SSL, which may cause
    // password prompts on macOS.
    qputenv("QT_SSL_USE_TEMPORARY_KEYCHAIN", QByteArray("1"));

#ifdef Q_OS_WIN32
    // On Windows, use ANGLE so we don't have to load both DX and OGL
    // user-mode drivers into our app.
    qputenv("QT_OPENGL", "angle");
#endif

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
    qmlRegisterSingletonType<AutoUpdateChecker>("AutoUpdateChecker", 1, 0,
                                                "AutoUpdateChecker",
                                                [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                    return new AutoUpdateChecker();
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

    // Avoid the default behavior of changing the timer resolution to 1 ms.
    // We don't want this all the time that Moonlight is open. We will set
    // it manually when we start streaming.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");

    int err = app.exec();

    SDL_Quit();

    return err;
}
