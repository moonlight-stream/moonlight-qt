#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // MacOS directive to prevent the menu bar from being merged into the native bar
    // i.e. it's in the window, and not the top left of the screen
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();

}
