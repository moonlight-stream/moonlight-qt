#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include "gui/computermodel.h"
#include "gui/appmodel.h"

// Don't let SDL hook our main function, since Qt is already
// doing the same thing
#define SDL_MAIN_HANDLED
#include <SDL.h>

int main(int argc, char *argv[])
{
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

    // Register our C++ types for QML
    qmlRegisterType<ComputerModel>("ComputerModel", 1, 0, "ComputerModel");
    qmlRegisterType<AppModel>("AppModel", 1, 0, "AppModel");

    // Load the main.qml file
    QQmlApplicationEngine engine;
    engine.load(QUrl(QStringLiteral("qrc:/gui/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    // Ensure that SDL is always initialized since we may need to use it
    // for non-streaming purposes (like checking on audio devices)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO |
                 SDL_INIT_AUDIO |
                 SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_Init() failed: %s",
                     SDL_GetError());
    }

    int err = app.exec();

    SDL_Quit();

    return err;
}
