#include "path.h"

#include <QtDebug>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QCoreApplication>

QString Path::s_LogDir;
QString Path::s_BoxArtCacheDir;
const QString Path::k_GameControllerMappingFile("gamecontrollerdb.txt");

QString Path::getLogDir()
{
    Q_ASSERT(!s_LogDir.isEmpty());
    return s_LogDir;
}

QString Path::getBoxArtCacheDir()
{
    Q_ASSERT(!s_BoxArtCacheDir.isEmpty());
    return s_BoxArtCacheDir;
}

QString Path::getGamepadMappingFile()
{
    QString candidatePath;

    // Check the current directory first
    candidatePath = QDir(QDir::currentPath()).absoluteFilePath(k_GameControllerMappingFile);
    if (QFile::exists(candidatePath)) {
        return candidatePath;
    }

    // Now check the data directories (for Linux, in particular)
    candidatePath = QStandardPaths::locate(QStandardPaths::DataLocation, k_GameControllerMappingFile);
    if (!candidatePath.isEmpty() && QFile::exists(candidatePath)) {
        return candidatePath;
    }

    // Now try the directory of our app installation (for Windows, if current dir doesn't find it)
    candidatePath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(k_GameControllerMappingFile);
    if (QFile::exists(candidatePath)) {
        return candidatePath;
    }

    // Now try the Resources folder in our app bundle for macOS
    QDir dir = QDir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cd("Resources");
    dir.filePath(k_GameControllerMappingFile);
    candidatePath = dir.absoluteFilePath(k_GameControllerMappingFile);
    if (QFile::exists(candidatePath)) {
        return candidatePath;
    }

    // Couldn't find it
    return QString();
}

void Path::initialize(bool portable)
{
    if (portable) {
        s_LogDir = QDir::currentPath();
        s_BoxArtCacheDir = QDir::currentPath() + "/boxart";
    }
    else {
#ifdef Q_OS_DARWIN
        // On macOS, $TMPDIR is some random folder under /var/folders/ that nobody can
        // easily find, so use the system's global tmp directory instead.
        s_LogDir = "/tmp";
#else
        s_LogDir = QDir::tempPath();
#endif
        s_BoxArtCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/boxart";
    }
}
