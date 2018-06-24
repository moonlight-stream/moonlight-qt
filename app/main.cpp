#include "gui/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // MacOS directive to prevent the menu bar from being merged into the native bar
    // i.e. it's in the window, and not the top left of the screen
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);

    // This avoids using the default keychain for SSL, which may cause
    // password prompts on macOS.
    qputenv("QT_SSL_USE_TEMPORARY_KEYCHAIN", QByteArray("1"));

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();

}
