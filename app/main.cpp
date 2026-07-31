#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QIcon>
#include <QQuickStyle>
#include <QMutex>
#include <QtDebug>
#include <QNetworkProxyFactory>
#include <QPalette>
#include <QFont>
#include <QCursor>
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QLocale>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <sys/socket.h>
#include <signal.h>
#endif

// Don't let SDL hook our main function, since Qt is already
// doing the same thing. This needs to be before any headers
// that might include SDL.h themselves.
#define SDL_MAIN_HANDLED
#include "SDL_compat.h"

#ifdef HAVE_FFMPEG
#include "streaming/video/ffmpeg.h"
#endif

#if defined(Q_OS_WIN32)
#include "antihookingprotection.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <dxgi1_6.h>
#elif defined(Q_OS_LINUX)
#include <openssl/ssl.h>
#endif

static QString getStartupApplicationDir(const char* argv0)
{
#if defined(Q_OS_WIN32)
    WCHAR modulePath[MAX_PATH];
    DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (modulePathLength > 0 && modulePathLength < MAX_PATH) {
        return QFileInfo(QString::fromWCharArray(modulePath, modulePathLength)).absolutePath();
    }
#endif

    QString programPath = QString::fromLocal8Bit(argv0 ? argv0 : "");
    if (!programPath.isEmpty()) {
        QFileInfo programInfo(programPath);
        if (programInfo.isAbsolute()) {
            return programInfo.absolutePath();
        }

        if (programPath.contains('/') || programPath.contains('\\')) {
            return QFileInfo(QDir::current(), programPath).absolutePath();
        }

        QString resolvedExecutable = QStandardPaths::findExecutable(programPath);
        if (!resolvedExecutable.isEmpty()) {
            return QFileInfo(resolvedExecutable).absolutePath();
        }
    }

    return QDir::currentPath();
}

#include "cli/listapps.h"
#include "cli/quitstream.h"
#include "cli/startstream.h"
#include "cli/pair.h"
#include "cli/commandlineparser.h"
#include "path.h"
#include "utils.h"
#include "gui/computermodel.h"
#include "gui/appmodel.h"
#include "backend/autoupdatechecker.h"
#include "backend/computermanager.h"
#include "backend/systemproperties.h"
#include "streaming/session.h"
#include "settings/streamingpreferences.h"
#include "gui/sdlgamepadkeynavigation.h"
#include "imageutils.h"
#include "streaming/macpermissions.h"
#ifdef Q_OS_DARWIN
#include "gui/macwindowchrome.h"
#endif

#ifdef Q_OS_WIN32
// 只有 Windows 分支的 app.setFont() 会用到它。不加这层 #ifdef 的话，其他平台每次
// 构建都会报一条 -Wunused-function。
static bool shouldUseChineseWindowsUiFont(StreamingPreferences::Language language)
{
    switch (language) {
    case StreamingPreferences::LANG_ZH_CN:
    case StreamingPreferences::LANG_ZH_TW:
        return true;
    case StreamingPreferences::LANG_AUTO:
        return QLocale::system().language() == QLocale::Chinese;
    default:
        return false;
    }
}
#endif

#if defined(Q_OS_WIN32)
#define IS_UNSPECIFIED_HANDLE(x) ((x) == INVALID_HANDLE_VALUE || (x) == NULL)

// Log to file or console dynamically for Windows builds
#define LOG_TO_FILE
#elif !defined(QT_DEBUG) && defined(Q_OS_DARWIN)
// Log to file for release Mac builds
#define LOG_TO_FILE
#else
// Log to console for debug Mac builds
#endif

// StreamUtils::setAsyncLogging() exposes control of this to the Session
// class to enable async logging once the stream has started.
//
// FIXME: Clean this up
QAtomicInt g_AsyncLoggingEnabled;

static QElapsedTimer s_LoggerTime;
static QTextStream s_LoggerStream(stderr);
static QThreadPool s_LoggerThread;
static QMutex s_SyncLoggerMutex;
static bool s_SuppressVerboseOutput;
static QRegularExpression k_RikeyRegex("&rikey=\\w+");
static QRegularExpression k_RikeyIdRegex("&rikeyid=[\\d-]+");
#ifdef LOG_TO_FILE
// Max log file size of 10 MB
static const uint64_t k_MaxLogSizeBytes = 10 * 1024 * 1024;
static QAtomicInteger<uint64_t> s_LogBytesWritten = 0;
static QFile* s_LoggerFile;
#endif

#ifdef HAVE_DRM_MASTER_HOOKS
extern "C" bool g_DisableDrmHooks;
#endif

class LoggerTask : public QRunnable
{
public:
    LoggerTask(const QString& msg) : m_Msg(msg)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        // QTextStream is not thread-safe, so we must lock. This will generally
        // only contend in synchronous logging mode or during a transition
        // between synchronous and asynchronous. Asynchronous won't contend in
        // the common case because we only have a single logging thread.
        QMutexLocker locker(&s_SyncLoggerMutex);
        s_LoggerStream << m_Msg;
        s_LoggerStream.flush();
    }

private:
    QString m_Msg;
};

void logToLoggerStream(QString& message)
{
#if defined(QT_DEBUG) && defined(Q_OS_WIN32)
    // Output log messages to a debugger if attached
    if (IsDebuggerPresent()) {
        thread_local QString lineBuffer;
        lineBuffer += message;
        if (message.endsWith('\n')) {
            OutputDebugStringW(lineBuffer.toStdWString().c_str());
            lineBuffer.clear();
        }
    }
#endif

    // Strip session encryption keys and IVs from the logs
    message.replace(k_RikeyRegex, "&rikey=REDACTED");
    message.replace(k_RikeyIdRegex, "&rikeyid=REDACTED");

#ifdef LOG_TO_FILE
    auto oldLogSize = s_LogBytesWritten.fetchAndAddRelaxed(message.size());
    if (oldLogSize >= k_MaxLogSizeBytes) {
        return;
    }
    else if (oldLogSize >= k_MaxLogSizeBytes - message.size()) {
        // Write one final message
        message = "Log size limit reached!";
    }
#endif

    if (g_AsyncLoggingEnabled) {
        // Queue the log message to be written asynchronously
        s_LoggerThread.start(new LoggerTask(message));
    }
    else {
        // Log the message immediately
        LoggerTask(message).run();
    }
}

void sdlLogToDiskHandler(void*, int category, SDL_LogPriority priority, const char* message)
{
    QString priorityTxt;

    switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
        if (s_SuppressVerboseOutput) {
            return;
        }
        priorityTxt = "Verbose";
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        if (s_SuppressVerboseOutput) {
            return;
        }
        priorityTxt = "Debug";
        break;
    case SDL_LOG_PRIORITY_INFO:
        if (s_SuppressVerboseOutput) {
            return;
        }
        priorityTxt = "Info";
        break;
    case SDL_LOG_PRIORITY_WARN:
        if (s_SuppressVerboseOutput) {
            return;
        }
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
    QString txt = QString("%1 - SDL %2 (%3): %4\n").arg(logTime.toString()).arg(priorityTxt).arg(category).arg(message);

    logToLoggerStream(txt);
}

void qtLogToDiskHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    QString typeTxt;

    switch (type) {
    case QtDebugMsg:
        if (s_SuppressVerboseOutput) {
            return;
        }
        typeTxt = "Debug";
        break;
    case QtInfoMsg:
        if (s_SuppressVerboseOutput) {
            return;
        }
        typeTxt = "Info";
        break;
    case QtWarningMsg:
        if (s_SuppressVerboseOutput) {
            return;
        }
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
    QString txt = QString("%1 - Qt %2: %3\n").arg(logTime.toString()).arg(typeTxt).arg(msg);

    logToLoggerStream(txt);
}

#ifdef HAVE_FFMPEG

void ffmpegLogToDiskHandler(void* ptr, int level, const char* fmt, va_list vl)
{
    char lineBuffer[1024];
    static int printPrefix = 1;

    if ((level & 0xFF) > av_log_get_level()) {
        return;
    }
    else if ((level & 0xFF) > AV_LOG_WARNING && s_SuppressVerboseOutput) {
        return;
    }

    // We need to use the *previous* printPrefix value to determine whether to
    // print the prefix this time. av_log_format_line() will set the printPrefix
    // value to indicate whether the prefix should be printed *next time*.
    bool shouldPrefixThisMessage = printPrefix != 0;

    av_log_format_line(ptr, level, fmt, vl, lineBuffer, sizeof(lineBuffer), &printPrefix);

    if (shouldPrefixThisMessage) {
        QTime logTime = QTime::fromMSecsSinceStartOfDay(s_LoggerTime.elapsed());
        QString txt = QString("%1 - FFmpeg: %2").arg(logTime.toString()).arg(lineBuffer);
        logToLoggerStream(txt);
    }
    else {
        QString txt = QString(lineBuffer);
        logToLoggerStream(txt);
    }
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
    QString qDmpFileName = QString::fromUtf16((const char16_t*)dmpFileName);
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

    // Sleep for a moment to allow the logging thread to finish up before crashing
    if (g_AsyncLoggingEnabled) {
        Sleep(500);
    }

    // Let the program crash and WER collect a dump
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

#ifdef Q_OS_UNIX

static int signalFds[2];

void handleSignal(int sig)
{
    send(signalFds[0], &sig, sizeof(sig), 0);
}

int SDLCALL signalHandlerThread(void* data)
{
    Q_UNUSED(data);

    Session* lastSession = nullptr;
    bool requestedQuit = false;

    int sig;
    while (recv(signalFds[1], &sig, sizeof(sig), MSG_WAITALL) == sizeof(sig)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Received signal: %d", sig);

        Session* session;
        switch (sig) {
        case SIGINT:
        case SIGTERM:
            // Check if we have an active streaming session
            session = Session::get();
            if (session != nullptr) {
                // Exit immediately if we haven't changed state since last attempt
                if (session == lastSession || requestedQuit) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Exiting immediately on second signal");
                    _Exit(1);
                }

                if (sig == SIGTERM) {
                    // If this is a SIGTERM, set the flag to quit
                    session->setShouldExit();
                    requestedQuit = true;
                }

                // Stop the streaming session
                session->interrupt();
                lastSession = session;
            }
            else {
                // Exit immediately if we haven't changed state since last attempt
                if (requestedQuit) {
                    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Exiting immediately on second signal");
                    _Exit(1);
                }

                // If we're not streaming, we'll close the whole app
                QCoreApplication::instance()->quit();
                requestedQuit = true;
            }
            break;

        default:
            Q_UNREACHABLE();
        }
    }

    return 0;
}

void configureSignalHandlers()
{
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, signalFds) == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "socketpair() failed: %d",
                     errno);
        return;
    }

    // Create a thread to handle our signals safely outside of signal context
    SDL_Thread* thread = SDL_CreateThread(signalHandlerThread, "Signal Handler", nullptr);
    SDL_DetachThread(thread);

    struct sigaction sa = {};
    sa.sa_handler = handleSignal;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

#endif

int main(int argc, char *argv[])
{
    SDL_SetMainReady();

    const QString startupApplicationDir = getStartupApplicationDir(argc > 0 ? argv[0] : nullptr);

    // Set the app version for the QCommandLineParser's showVersion() command
    QCoreApplication::setApplicationVersion(VERSION_STR);

    // Set these here to allow us to use the default QSettings constructor.
    // These also ensure that our cache directory is named correctly. As such,
    // it is critical that these be called before Path::initialize().
    QCoreApplication::setOrganizationName("Moonlight Game Streaming Project");
    QCoreApplication::setOrganizationDomain("moonlight-stream.com");
    QCoreApplication::setApplicationName("Moonlight");

    if (QFile(QDir(startupApplicationDir).filePath("portable.dat")).exists()) {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, startupApplicationDir);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, startupApplicationDir);

        // Initialize paths for portable mode
        Path::initialize(true, startupApplicationDir);
    }
    else {
        // Initialize paths for standard installation
        Path::initialize(false);
    }

    // Override the default QML cache directory with the one we chose
    if (qEnvironmentVariableIsEmpty("QML_DISK_CACHE_PATH")) {
        qputenv("QML_DISK_CACHE_PATH", Path::getQmlCacheDir().toUtf8());
    }

#ifdef Q_OS_WIN32
    // Grab the original std handles before we potentially redirect them later
    HANDLE oldConOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE oldConErr = GetStdHandle(STD_ERROR_HANDLE);
#endif

#ifdef LOG_TO_FILE
    QDir tempDir(Path::getLogDir());

#ifdef Q_OS_WIN32
    // Only log to a file if the user didn't redirect stderr somewhere else
    if (IS_UNSPECIFIED_HANDLE(oldConErr))
#endif
    {
        s_LoggerFile = new QFile(tempDir.filePath(QString("Moonlight-%1.log").arg(QDateTime::currentSecsSinceEpoch())));
        if (s_LoggerFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream(stderr) << "Redirecting log output to " << s_LoggerFile->fileName() << Qt::endl;
            s_LoggerStream.setDevice(s_LoggerFile);
        }
    }
#endif

    // Serialize log messages on a single thread
    s_LoggerThread.setMaxThreadCount(1);
    s_LoggerTime.start();

    // Register our logger with all libraries
#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_SetLogOutputFunction(sdlLogToDiskHandler, nullptr);
#else
    SDL_LogOutputFunction oldSdlLogFn;
    void* oldSdlLogUserdata;
    SDL_LogGetOutputFunction(&oldSdlLogFn, &oldSdlLogUserdata);
    SDL_LogSetOutputFunction(sdlLogToDiskHandler, nullptr);
#endif
    qInstallMessageHandler(qtLogToDiskHandler);
#ifdef HAVE_FFMPEG
    av_log_set_callback(ffmpegLogToDiskHandler);
#endif

#ifdef Q_OS_WIN32
    // Create a crash dump when we crash on Windows
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif

#ifdef LOG_TO_FILE
    // Prune the oldest existing logs if there are more than 10
    QStringList existingLogNames = tempDir.entryList(QStringList("Moonlight-*.log"), QDir::NoFilter, QDir::SortFlag::Time);
    for (int i = 10; i < existingLogNames.size(); i++) {
        qInfo() << "Removing old log file:" << existingLogNames.at(i);
        QFile(tempDir.filePath(existingLogNames.at(i))).remove();
    }
#endif

#if defined(Q_OS_WIN32)
    // Force AntiHooking.dll to be statically imported and loaded
    // by ntdll on Win32 platforms by calling a dummy function.
    AntiHookingDummyImport();
#elif defined(APP_IMAGE)
    // Force libssl.so to be directly linked to our binary, so
    // linuxdeployqt can find it and include it in our AppImage.
    // QtNetwork will pull it in via dlopen().
    SSL_free(nullptr);
#endif

    // We keep this at function scope to ensure it stays around while we're running,
    // because the Qt QPA will need to read it. Since the temporary file is only
    // created when open() is called, this doesn't do any harm for other platforms.
    QTemporaryFile eglfsConfigFile;

    // Avoid using High DPI on EGLFS. It breaks font rendering.
    // https://bugreports.qt.io/browse/QTBUG-64377
    //
    // NB: We can't use QGuiApplication::platformName() here because it is only
    // set once the QGuiApplication is created, which is too late to enable High DPI :(
    if (WMUtils::isRunningWindowManager()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // Enable High DPI support on Qt 5.x. It is always enabled on Qt 6.0
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        // Enable fractional High DPI scaling on Qt 5.14 and later
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    }
    else {
#ifndef STEAM_LINK
        if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
            qInfo() << "Unable to detect Wayland or X11, so EGLFS will be used by default. Set QT_QPA_PLATFORM to override this.";
            qputenv("QT_QPA_PLATFORM", "eglfs");

            if (!qEnvironmentVariableIsSet("QT_QPA_EGLFS_ALWAYS_SET_MODE")) {
                qInfo() << "Setting display mode by default. Set QT_QPA_EGLFS_ALWAYS_SET_MODE=0 to override this.";

                // The UI doesn't appear on RetroPie without this option.
                qputenv("QT_QPA_EGLFS_ALWAYS_SET_MODE", "1");
            }

            if (!QFile("/dev/dri").exists()) {
                qWarning() << "Unable to find a KMSDRM display device!";
                qWarning() << "On the Raspberry Pi, you must enable the 'fake KMS' driver in raspi-config to use Moonlight outside of the GUI environment.";
            }
            else if (!qEnvironmentVariableIsSet("QT_QPA_EGLFS_KMS_CONFIG")) {
                // HACK: Remove this when Qt is fixed to properly check for display support before picking a card
                QString cardOverride = WMUtils::getDrmCardOverride();
                if (!cardOverride.isEmpty()) {
                    if (eglfsConfigFile.open()) {
                        qInfo() << "Overriding default Qt EGLFS card selection to" << cardOverride;
                        QTextStream(&eglfsConfigFile) << "{ \"device\": \"" << cardOverride << "\" }";
                        qputenv("QT_QPA_EGLFS_KMS_CONFIG", eglfsConfigFile.fileName().toUtf8());
                        eglfsConfigFile.close();
                    }
                }
            }
        }

        // EGLFS uses OpenGLES 2.0, so we will too. Some embedded platforms may not
        // even have working OpenGL implementations, so GLES is the only option.
        // See https://github.com/moonlight-stream/moonlight-qt/issues/868
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
#endif
    }

    bool forceGles;
    if (!Utils::getEnvironmentVariableOverride("FORCE_QT_GLES", &forceGles)) {
        forceGles = WMUtils::isRunningNvidiaProprietaryDriverX11() ||
                    !WMUtils::supportsDesktopGLWithEGL();
    }
    if (forceGles) {
        // The Nvidia proprietary driver causes Qt to render a black window when using
        // the default Desktop GL profile with EGL. AS a workaround, we default to
        // OpenGL ES when running on Nvidia on X11.
        // https://qt-project.atlassian.net/browse/QTBUG-106065
        QSurfaceFormat fmt;
        fmt.setRenderableType(QSurfaceFormat::OpenGLES);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    // Some ARM and RISC-V embedded devices don't have working GLX which can cause
    // SDL to fail to find a working OpenGL implementation at all. Let's force EGL
    // on all platforms for both SDL and Qt. This also avoids GLX-EGL interop issues
    // when trying to use EGL on the main thread after Qt uses GLX.
    SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "1");
    qputenv("QT_XCB_GL_INTEGRATION", "xcb_egl");

#ifdef Q_OS_WIN32
    // Let us see the true VBlank rather than DWM's approximation. We do this here
    // because this API must be called before the first swapchain (which Qt will
    // create when the window is displayed). This is supported on Win11 22H2+.
    auto fnDXGIDisableVBlankVirtualization =
        (decltype(DXGIDisableVBlankVirtualization)*)GetProcAddress(GetModuleHandleW(L"dxgi.dll"),
                                                                   "DXGIDisableVBlankVirtualization");
    if (fnDXGIDisableVBlankVirtualization) {
        fnDXGIDisableVBlankVirtualization();
    }
#endif

#ifdef Q_OS_MACOS
    // This avoids using the default keychain for SSL, which may cause
    // password prompts on macOS.
    qputenv("QT_SSL_USE_TEMPORARY_KEYCHAIN", "1");
#endif

#if defined(Q_OS_WIN32) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (!qEnvironmentVariableIsSet("QT_OPENGL")) {
        // On Windows, use ANGLE so we don't have to load OpenGL
        // user-mode drivers into our app. OGL drivers (especially Intel)
        // seem to crash Moonlight far more often than DirectX.
        qputenv("QT_OPENGL", "angle");
    }
#endif

#if !defined(Q_OS_WIN32)
    // Other platforms retain the established non-threaded behavior because
    // streaming code may block the main thread while pumping events.
    if (!qEnvironmentVariableIsSet("QSG_RENDER_LOOP")) {
        qputenv("QSG_RENDER_LOOP", "basic");
    }
#endif

#if defined(Q_OS_DARWIN) && defined(QT_DEBUG) && !defined(HAVE_LIBPLACEBO_VULKAN)
    // Enable Metal valiation for debug builds without libplacebo
    //
    // The current MoltenVK driver as of Vulkan SDK 1.4.350 triggers Metal debug layer
    // violations on frame and overlay uploads like:
    // _validateReplaceRegion:252: failed assertion `Replace Region Validation
    // bytesPerRow(4803) must be a multiple of MTLPixelFormatBGRA8Unorm pixel bytes(4).
    qputenv("MTL_DEBUG_LAYER", "1");
    qputenv("MTL_SHADER_VALIDATION", "1");
#endif

    // We don't want system proxies to apply to us
    QNetworkProxyFactory::setUseSystemConfiguration(false);

    // Clear any default application proxy
    QNetworkProxy noProxy(QNetworkProxy::NoProxy);
    QNetworkProxy::setApplicationProxy(noProxy);

    // Register custom metatypes for use in signals
    qRegisterMetaType<NvApp>("NvApp");

    // Allow the display to sleep by default. We will manually use SDL_DisableScreenSaver()
    // and SDL_EnableScreenSaver() when appropriate. This hint must be set before
    // initializing the SDL video subsystem to have any effect.
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");

#ifdef Q_OS_DARWIN
    // SDL reads this hint when the video subsystem is first initialized. Set
    // the saved preference here so the hardware capability probe cannot lock
    // in the default mouse-only behavior before a streaming session starts.
    SDL_SetHint(SDL_HINT_TRACKPAD_IS_TOUCH_ONLY,
                StreamingPreferences::get()->enableNativeTouchpad ? "1" : "0");
#endif

    // We use MMAL to render on Raspberry Pi, so we do not require DRM master.
    SDL_SetHint(SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER, "0");

    // Use Direct3D 9Ex to avoid a deadlock caused by the D3D device being reset when
    // the user triggers a UAC prompt. This option controls the software/SDL renderer.
    // The DXVA2 renderer uses Direct3D 9Ex itself directly.
    SDL_SetHint(SDL_HINT_WINDOWS_USE_D3D9EX, "1");

    if (SDL_InitSubSystem(SDL_INIT_TIMER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_TIMER) failed: %s",
                     SDL_GetError());
        return -1;
    }

#if defined(STEAM_LINK) || defined(Q_OS_WIN32)
    // Steam Link requires that we initialize video before creating our
    // QGuiApplication in order to configure the framebuffer correctly.
    //
    // We keep the video subsystem initialized on Windows because it's
    // much more costly to reinitialize than other platforms. It hurts
    // the settings page transition performance significantly.
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s",
                     SDL_GetError());
        return -1;
    }
#endif

    // Use atexit() to ensure SDL_Quit() is called. This avoids
    // racing with object destruction where SDL may be used.
    atexit(SDL_Quit);

    // Avoid the default behavior of changing the timer resolution to 1 ms.
    // We don't want this all the time that Moonlight is open. We will set
    // it manually when we start streaming.
    SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "0");

    // Disable minimize on focus loss by default. Users seem to want this off by default.
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

    // SDL 2.0.12 changes the default behavior to use the button label rather than the button
    // position as most other software does. Set this back to 0 to stay consistent with prior
    // releases of Moonlight.
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");

    // Disable relative mouse scaling to renderer size or logical DPI. We want to send
    // the mouse motion exactly how it was given to us.
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SCALING, "0");

    // Set our app name for SDL to use with PulseAudio and PipeWire. This matches what we
    // provide as our app name to libsoundio too. On SDL 2.0.18+, SDL_APP_NAME is also used
    // for screensaver inhibitor reporting.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME, "Moonlight");
    SDL_SetHint(SDL_HINT_APP_NAME, "Moonlight");

    // SDL will try to lock the mouse cursor on Wayland if it's not visible in order to
    // support applications that assume they can warp the cursor (which isn't possible
    // on Wayland). We don't want this behavior because it interferes with seamless mouse
    // mode when toggling between windowed and fullscreen modes by unexpectedly locking
    // the mouse cursor.
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_EMULATE_MOUSE_WARP, "0");

#ifdef QT_DEBUG
    // Allow thread naming using exceptions on debug builds. SDL doesn't use SEH
    // when throwing the exceptions, so we don't enable it for release builds out
    // of caution.
    SDL_SetHint(SDL_HINT_WINDOWS_DISABLE_THREAD_NAMING, "0");
#endif

    // Enable fast parameter checks on SDL 3.4.0+. We don't abuse the API by passing
    // incorrect objects, so we don't need additional expensive parameter checks.
    SDL_SetHint("SDL_INVALID_PARAM_CHECKS", "1");

    // Disable hotplug detection for SDL_GetKeyboards() and SDL_GetMice(). We don't
    // use this functionality and it can cause hangs when querying broken devices.
    SDL_SetHint("SDL_WINDOWS_DETECT_DEVICE_HOTPLUG", "0");

    // SDL3 supports offloading scaling to the Wayland compositor, which we take
    // advantage of in the GL_IS_SLOW case to help fillrate-limited GPUs. To stay
    // consistent with our own scaling logic, we need aspect ratio scaling which
    // KDE doesn't currently handle properly. As a compromise, we'll just enable
    // aspect ratio scaling in non-KDE environments.
    //
    // NB: We do not force SDL_VIDEO_WAYLAND_MODE_SCALING to "stretch" on KDE,
    // because SDL 3.6 has a workaround for KDE and switches the default to
    // "aspect" for all desktops.
    if (qgetenv("XDG_CURRENT_DESKTOP") != "KDE") {
        SDL_SetHint("SDL_VIDEO_WAYLAND_MODE_SCALING", "aspect");
    }

    QGuiApplication app(argc, argv);

#ifdef Q_OS_DARWIN
    // macOS defaults "Keyboard navigation" to text fields and lists only, which
    // prevents Tab (and the gamepad navigation that synthesizes it) from moving
    // focus between non-text controls on the settings page. Force Tab to reach
    // all controls so keyboard and gamepad UI navigation work without requiring
    // the user to enable a system accessibility setting. Other platforms already
    // default to this behavior.
    app.styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
#endif

#ifdef Q_OS_UNIX
    // Register signal handlers to arbitrate between SDL and Qt.
    // NB: This has to be done after the QGuiApplication is constructed to
    // ensure Qt has already installed its VT signals before we override
    // some of them with our own.
    configureSignalHandlers();
#endif

#ifdef Q_OS_WIN32
    // If we don't have stdout or stderr handles (which will normally be the case
    // since we're a /SUBSYSTEM:WINDOWS app), attach to our parent console and use
    // that for stdout and stderr.
    //
    // If we do have stdout or stderr handles, that means the user has used standard
    // handle redirection. In that case, we don't want to override those handles.
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // If we didn't have an old stdout/stderr handle, use the new CONOUT$ handle
        if (IS_UNSPECIFIED_HANDLE(oldConOut)) {
            FILE* fp;
            if (freopen_s(&fp, "CONOUT$", "w", stdout) == 0) {
                setvbuf(fp, NULL, _IONBF, 0);
            }
            else {
                freopen_s(&fp, "NUL", "w", stdout);
            }
        }
        if (IS_UNSPECIFIED_HANDLE(oldConErr)) {
            FILE* fp;
            if (freopen_s(&fp, "CONOUT$", "w", stderr) == 0) {
                setvbuf(fp, NULL, _IONBF, 0);
            }
            else {
                freopen_s(&fp, "NUL", "w", stderr);
            }
        }
    }
#endif

    GlobalCommandLineParser parser;
    GlobalCommandLineParser::ParseResult commandLineParserResult = parser.parse(app.arguments());
    switch (commandLineParserResult) {
    case GlobalCommandLineParser::ListRequested:
        // Don't log to the console since it will jumble the command output
        s_SuppressVerboseOutput = true;
        break;
    default:
        break;
    }

    SDL_version compileVersion;
    SDL_VERSION(&compileVersion);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Compiled with SDL %d.%d.%d",
                compileVersion.major, compileVersion.minor, compileVersion.patch);

    SDL_version runtimeVersion;
    SDL_GetVersion(&runtimeVersion);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "Running with SDL %d.%d.%d",
                runtimeVersion.major, runtimeVersion.minor, runtimeVersion.patch);

    // If we're running under sdl2-compat, it may tell us the underlying SDL3 version
    const char* sdl3Version = SDL_GetHint("SDL3_VERSION");
    int sdl3VersionInt = 0;
    if (sdl3Version) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "SDL3 version: %s",
                    sdl3Version);

        // Parse the version into integer form
        QStringList list = QString(sdl3Version).split('.');
        Q_ASSERT(list.size() == 3);
        if (list.size() == 3) {
            sdl3VersionInt = SDL_VERSIONNUM(list.at(0).toInt(), list.at(1).toInt(), list.at(2).toInt());
        }
    }

    // SDL 3.4.0 and 3.4.2 have bugs in atomic KMSDRM support that break us,
    // so disable atomic on the affected SDL3 versions. Since not all versions
    // of sdl2-compat will set the SDL3_VERSION hint, we assume that versions
    // prior to 2.32.66 are affected (since that was released at the same time
    // as SDL 3.4.4 with the atomic fixes).
    if ((sdl3VersionInt != 0 && sdl3VersionInt < SDL_VERSIONNUM(3, 4, 4)) ||
            (runtimeVersion.patch >= 50 && runtimeVersion.patch < 66)) {
#if !defined(Q_OS_WIN32) && !defined(Q_OS_DARWIN)
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Setting SDL_KMSDRM_ATOMIC=0 for older sdl2-compat/SDL3 version");
        SDL_SetHint("SDL_KMSDRM_ATOMIC", "0");
#endif
    }

    // Apply the initial translation based on user preference
    StreamingPreferences::get()->retranslate();

    // Trickily declare the translation for dialog buttons
    QCoreApplication::translate("QPlatformTheme", "&Yes");
    QCoreApplication::translate("QPlatformTheme", "&No");
    QCoreApplication::translate("QPlatformTheme", "OK");
    QCoreApplication::translate("QPlatformTheme", "Help");
    QCoreApplication::translate("QPlatformTheme", "Cancel");

    // After the QGuiApplication is created, the platform stuff will be initialized
    // and we can set the SDL video driver to match Qt.
    if (QGuiApplication::platformName() == "xcb") {
        if (WMUtils::isRunningWayland()) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Detected XWayland. This will probably break hardware decoding! Try running with QT_QPA_PLATFORM=wayland or switch to X11.");
        }
        qputenv("SDL_VIDEODRIVER", "x11");
    }
    else if (QGuiApplication::platformName().startsWith("wayland")) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Detected Wayland");
        qputenv("SDL_VIDEODRIVER", "wayland");
    }
#ifndef STEAM_LINK
    // Force use of the KMSDRM backend for SDL when using Qt platform plugins
    // that directly draw to the display without a windowing system.
    else if (QGuiApplication::platformName() == "eglfs" || QGuiApplication::platformName() == "linuxfb") {
        qputenv("SDL_VIDEODRIVER", "kmsdrm");
    }
#endif

#ifdef HAVE_DRM_MASTER_HOOKS
    // Only use the Qt-SDL DRM master interoperability hooks if Qt is using KMS
    g_DisableDrmHooks = QGuiApplication::platformName() != "eglfs";
#endif

#ifdef STEAM_LINK
    // Qt 5.9 from the Steam Link SDK is not able to load any fonts
    // since the Steam Link doesn't include any of the ones it looks
    // for. We know it has NotoSans so we will explicitly ask for that.
    if (app.font().family().isEmpty()) {
        qWarning() << "SL HACK: No default font - using NotoSans";

        QFont fon("NotoSans");
        app.setFont(fon);
    }

    // Move the mouse to the bottom right so it's invisible when using
    // gamepad-only navigation.
    QCursor().setPos(0xFFFF, 0xFFFF);
#elif defined(Q_OS_WIN32)
    const QStringList fontFamilies = QFontDatabase::families();
    QString defaultFontFamily = QStringLiteral("Segoe UI");

    if (shouldUseChineseWindowsUiFont(StreamingPreferences::get()->language)) {
        if (fontFamilies.contains(QStringLiteral("Microsoft YaHei UI"))) {
            defaultFontFamily = QStringLiteral("Microsoft YaHei UI");
        }
        else if (fontFamilies.contains(QStringLiteral("Microsoft YaHei"))) {
            defaultFontFamily = QStringLiteral("Microsoft YaHei");
        }
    }

    QFont defaultFont(defaultFontFamily, 9);
    defaultFont.setStyleHint(QFont::SansSerif);
    if (fontFamilies.contains(defaultFontFamily)) {
        app.setFont(defaultFont);
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Set default font to %s", qPrintable(defaultFontFamily));
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s font not found, using system default", qPrintable(defaultFontFamily));
    }
#elif !SDL_VERSION_ATLEAST(2, 0, 11) && defined(Q_OS_LINUX) && (defined(__arm__) || defined(__aarch64__))
    if (qgetenv("SDL_VIDEO_GL_DRIVER").isEmpty() && QGuiApplication::platformName() == "eglfs") {
        // Look for Raspberry Pi GLES libraries. SDL 2.0.10 and earlier needs some help finding
        // the correct libraries for the KMSDRM backend if not compiled with the RPI backend enabled.
        if (SDL_LoadObject("libbrcmGLESv2.so") != nullptr) {
            qputenv("SDL_VIDEO_GL_DRIVER", "libbrcmGLESv2.so");
        }
        else if (SDL_LoadObject("/opt/vc/lib/libbrcmGLESv2.so") != nullptr) {
            qputenv("SDL_VIDEO_GL_DRIVER", "/opt/vc/lib/libbrcmGLESv2.so");
        }
    }
#endif

#ifdef Q_OS_DARWIN
    // Pre-request microphone permission at startup via AVCaptureDevice so that
    // only a single TCC dialog is shown. Universal (fat) binaries have distinct
    // CDHashes per architecture slice, and each QMediaDevices / CoreAudio call
    // can trigger a separate prompt. By requesting permission once through
    // AVCaptureDevice before any CoreAudio usage, macOS caches the grant for
    // the whole app.
    checkAndRequestMicrophonePermission();
#else
    // Set the window icon except on macOS where we want to keep the
    // modified macOS 11 style rounded corner icon.
    app.setWindowIcon(QIcon(":/res/moonlight.svg"));
#endif

    // This is necessary to show our icon correctly on Wayland
    app.setDesktopFileName("com.moonlight_stream.Moonlight");
    qputenv("SDL_VIDEO_WAYLAND_WMCLASS", "com.moonlight_stream.Moonlight");
    qputenv("SDL_VIDEO_X11_WMCLASS", "com.moonlight_stream.Moonlight");

    // Register our C++ types for QML
    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");
    qmlRegisterUncreatableType<Session>("Session", 1, 0, "Session", "Session cannot be created from QML");
    qmlRegisterSingletonType<ComputerManager>("ComputerManager", 1, 0,
                                              "ComputerManager",
                                              [](QQmlEngine* qmlEngine, QJSEngine*) -> QObject* {
                                                  return new ComputerManager(StreamingPreferences::get(qmlEngine));
                                              });
    qmlRegisterSingletonType<AutoUpdateChecker>("AutoUpdateChecker", 1, 0,
                                                "AutoUpdateChecker",
                                                [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                    return new AutoUpdateChecker();
                                                });
    qmlRegisterSingletonType<SystemProperties>("SystemProperties", 1, 0,
                                               "SystemProperties",
                                               [](QQmlEngine*, QJSEngine*) -> QObject* {
                                                   return new SystemProperties();
                                               });
    qmlRegisterSingletonType<SdlGamepadKeyNavigation>("SdlGamepadKeyNavigation", 1, 0,
                                                      "SdlGamepadKeyNavigation",
                                                      [](QQmlEngine* qmlEngine, QJSEngine*) -> QObject* {
                                                          return new SdlGamepadKeyNavigation(StreamingPreferences::get(qmlEngine));
                                                      });
    qmlRegisterSingletonType<StreamingPreferences>("StreamingPreferences", 1, 0,
                                                   "StreamingPreferences",
                                                   [](QQmlEngine* qmlEngine, QJSEngine*) -> QObject* {
                                                       return StreamingPreferences::get(qmlEngine);
                                                   });
    qmlRegisterType<ImageUtils>("ImageUtils", 1, 0, "ImageUtils");

    // Create the identity manager on the main thread
    IdentityManager::get();

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Qt 6.8+ ships the FluentWinUI3 style, which is what our settings UI is designed
    // around. It picks light/dark from the application color scheme (there is no env
    // var equivalent to the Material ones), and our icons are styled for a dark theme,
    // so we force dark here rather than following the system.
    QQuickStyle::setStyle("FluentWinUI3");
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);

    // Unlike the Material style, FluentWinUI3 takes all of its colors from the
    // application palette: ApplicationWindow is literally "color: palette.window",
    // and the checked color of check boxes, switches, sliders and progress bars
    // comes from palette.accent. setColorScheme() above only swaps the style's own
    // assets; on macOS the palette still follows the system appearance, so under a
    // light system theme every page we haven't restyled ourselves (the connection
    // spinner, the legacy settings groups) renders as white-on-white.
    //
    // Force a dark palette built from the alkaidlab.com design variables so the
    // whole app is consistent regardless of the system appearance.
    {
        const QColor background(0x0F, 0x17, 0x2A);  // --background-darker
        const QColor surface(0x1E, 0x29, 0x3B);     // --background-dark
        const QColor border(0x33, 0x41, 0x55);      // --border-dark
        const QColor text(0xF1, 0xF5, 0xF9);
        const QColor textMuted(0x94, 0xA3, 0xB8);   // --text-muted
        const QColor accent(0x39, 0xC5, 0xBB);      // --primary-color

        QPalette palette;
        palette.setColor(QPalette::Window, background);
        palette.setColor(QPalette::WindowText, text);
        palette.setColor(QPalette::Base, surface);
        palette.setColor(QPalette::AlternateBase, border);
        palette.setColor(QPalette::Text, text);
        palette.setColor(QPalette::Button, surface);
        palette.setColor(QPalette::ButtonText, text);
        palette.setColor(QPalette::BrightText, text);
        palette.setColor(QPalette::ToolTipBase, surface);
        palette.setColor(QPalette::ToolTipText, text);
        palette.setColor(QPalette::PlaceholderText, textMuted);
        palette.setColor(QPalette::Mid, border);
        palette.setColor(QPalette::Dark, background);
        palette.setColor(QPalette::Light, border);
        palette.setColor(QPalette::Midlight, border);
        palette.setColor(QPalette::Shadow, background);
        palette.setColor(QPalette::Accent, accent);
        palette.setColor(QPalette::Highlight, accent);
        palette.setColor(QPalette::HighlightedText, background);
        palette.setColor(QPalette::Link, accent);
        palette.setColor(QPalette::LinkVisited, accent);

        palette.setColor(QPalette::Disabled, QPalette::WindowText, textMuted);
        palette.setColor(QPalette::Disabled, QPalette::Text, textMuted);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, textMuted);

        QGuiApplication::setPalette(palette);
    }
#else
    // Fall back to the Material theme on older Qt builds
    QQuickStyle::setStyle("Material");

    // Our icons are styled for a dark theme, so we do not allow the user to override this
    qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark");

    // These are defaults that we allow the user to override
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_ACCENT")) {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "Purple");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_VARIANT")) {
        qputenv("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
    }
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_MATERIAL_PRIMARY")) {
        // Qt 6.9 began to use a different shade of Material.Indigo when we use a dark theme
        // (which is all the time). The new color looks washed out, so manually specify the
        // old primary color unless the user overrides it themselves.
        qputenv("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", "#3F51B5");
    }
#endif

    // 界面字体：Manrope（正文/标题）+ DM Mono（数字、状态徽标、宽字距微标签），
    // 这是 neo-brutalism 视觉的一半，见 app/res/fonts/README.md。
    //
    // 必须放在上面那整段样式/palette 块之后：Windows 分支在更前面已经调过一次
    // app.setFont()，谁最后调谁生效，顺序反了这里就白设了。
    {
        static const char* const kBundledFonts[] = {
            ":/res/fonts/Manrope-Regular.ttf",
            ":/res/fonts/Manrope-SemiBold.ttf",
            ":/res/fonts/Manrope-ExtraBold.ttf",
            ":/res/fonts/DMMono-Regular.ttf",
        };

        bool haveManrope = false;
        for (const char* path : kBundledFonts) {
            if (QFontDatabase::addApplicationFont(QLatin1String(path)) < 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load bundled font: %s", path);
            }
            else if (QLatin1String(path).startsWith(QLatin1String(":/res/fonts/Manrope-"))) {
                haveManrope = true;
            }
        }

        if (haveManrope) {
            // Manrope 和 DM Mono 都没有中文字形，中文交给系统字体回退。
            // Qt 会跳过列表里不存在的 family，所以这里可以无条件把候选都列上。
            QStringList families;
            families << QStringLiteral("Manrope");
#ifdef Q_OS_DARWIN
            families << QStringLiteral("PingFang SC");
#elif defined(Q_OS_WIN32)
            const QStringList preferredHanFamilies = {
                QStringLiteral("Microsoft YaHei UI"),
                QStringLiteral("Microsoft YaHei"),
                QStringLiteral("Noto Sans SC"),
                QStringLiteral("DengXian"),
            };
            families << preferredHanFamilies;
#else
            families << QStringLiteral("Noto Sans CJK SC") << QStringLiteral("Source Han Sans SC");
#endif
            // 最后兜住原本的系统默认字体，别把上面平台分支设好的字号/字形提示丢了
            QFont uiFont = app.font();
            families << uiFont.family();
            uiFont.setFamilies(families);
            uiFont.setStyleHint(QFont::SansSerif);
            app.setFont(uiFont);

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0) && defined(Q_OS_WIN32)
            // QML controls frequently select Manrope or DM Mono directly.
            // That replaces the default font's family list, so Windows may
            // choose SimSun for missing Han glyphs. Pin the application-wide
            // Han fallback to modern sans-serif fonts instead.
            QStringList hanFallbackFamilies;
            const QStringList installedFamilies = QFontDatabase::families();
            for (const QString &family : preferredHanFamilies) {
                if (installedFamilies.contains(family)) {
                    hanFallbackFamilies << family;
                }
            }
            if (!hanFallbackFamilies.isEmpty()) {
                QFontDatabase::setApplicationFallbackFontFamilies(QChar::Script_Han,
                                                                  hanFallbackFamilies);
            }
#endif

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "UI font families: %s",
                        qPrintable(families.join(QLatin1String(", "))));
        }
    }

    QQmlApplicationEngine engine;
    QString initialView;
    bool hasGUI = true;

    switch (commandLineParserResult) {
    case GlobalCommandLineParser::NormalStartRequested:
        initialView = "qrc:/gui/PcView.qml";
        break;
    case GlobalCommandLineParser::StreamRequested:
        {
            initialView = "qrc:/gui/CliStartStreamSegue.qml";
            StreamingPreferences* preferences = StreamingPreferences::get();
            StreamCommandLineParser streamParser;
            streamParser.parse(app.arguments(), preferences);
            QString host    = streamParser.getHost();
            QString appName = streamParser.getAppName();
            auto launcher   = new CliStartStream::Launcher(host, appName, preferences, &app);
            engine.rootContext()->setContextProperty("launcher", launcher);
            break;
        }
    case GlobalCommandLineParser::QuitRequested:
        {
            initialView = "qrc:/gui/CliQuitStreamSegue.qml";
            QuitCommandLineParser quitParser;
            quitParser.parse(app.arguments());
            auto launcher = new CliQuitStream::Launcher(quitParser.getHost(), &app);
            engine.rootContext()->setContextProperty("launcher", launcher);
            break;
        }
    case GlobalCommandLineParser::PairRequested:
        {
            initialView = "qrc:/gui/CliPair.qml";
            PairCommandLineParser pairParser;
            pairParser.parse(app.arguments());
            auto launcher = new CliPair::Launcher(pairParser.getHost(), pairParser.getPredefinedPin(), &app);
            engine.rootContext()->setContextProperty("launcher", launcher);
            break;
        }
    case GlobalCommandLineParser::ListRequested:
        {
            ListCommandLineParser listParser;
            listParser.parse(app.arguments());
            auto launcher = new CliListApps::Launcher(listParser.getHost(), listParser, &app);
            launcher->execute(new ComputerManager(StreamingPreferences::get()));
            hasGUI = false;
            break;
        }
    }

    if (hasGUI) {
        engine.rootContext()->setContextProperty("initialView", initialView);
        engine.rootContext()->setContextProperty("runConfigChecks", commandLineParserResult == GlobalCommandLineParser::NormalStartRequested);

        // Load the main.qml file
        engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
        if (engine.rootObjects().isEmpty())
            return -1;

#ifdef Q_OS_DARWIN
        // 主界面去掉了系统标题栏的底色，那条 56px 的工具栏就是标题栏。但系统的标题栏
        // 带子仍然只有 28~32pt 高：红绿灯挤在最上面一小条里，而 AppKit 也只在那条带子
        // 里提供窗口拖动和双击缩放，工具栏下半部分是拖不动的。把带子拉高到 56，
        // 红绿灯落到 bar 的中线上，拖动区也就覆盖了整条 bar。
        //
        // 56 要和 main.qml 里 toolBar 的 height 保持一致。
        if (auto* rootWindow = qobject_cast<QWindow*>(engine.rootObjects().first())) {
            MacWindowChrome::useTallTitleBar(rootWindow, 56);
        }
#endif
    }

    int err = app.exec();

    // Give worker tasks time to properly exit. Fixes PendingQuitTask
    // sometimes freezing and blocking process exit.
    QThreadPool::globalInstance()->waitForDone(30000);

    // Restore the default logger for all libraries before shutting down ours
#if SDL_VERSION_ATLEAST(3, 0, 0)
    SDL_SetLogOutputFunction(SDL_GetDefaultLogOutputFunction(), nullptr);
#else
    SDL_LogSetOutputFunction(oldSdlLogFn, oldSdlLogUserdata);
#endif
    qInstallMessageHandler(nullptr);
#ifdef HAVE_FFMPEG
    av_log_set_callback(av_log_default_callback);
#endif

    // We should not be in async logging mode anymore
    Q_ASSERT(g_AsyncLoggingEnabled == 0);

    // Wait for pending log messages to be printed
    s_LoggerThread.waitForDone();

#ifdef Q_OS_WIN32
    // Without an explicit flush, console redirection for the list command
    // doesn't work reliably (sometimes the target file contains no text).
    fflush(stderr);
    fflush(stdout);
#endif

    return err;
}
