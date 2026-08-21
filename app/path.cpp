#include "path.h"

#include <QtDebug>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QCoreApplication>

QString Path::s_CacheDir;
QString Path::s_LogDir;
QString Path::s_DumpDir;
QString Path::s_BoxArtCacheDir;
QString Path::s_QmlCacheDir;
QString Path::s_PortableRootDir;

QString Path::getLogDir()
{
    Q_ASSERT(!s_LogDir.isEmpty());
    return s_LogDir;
}

QString Path::getDumpDir()
{
    Q_ASSERT(!s_DumpDir.isEmpty());
    return s_DumpDir;
}

QString Path::getBoxArtCacheDir()
{
    Q_ASSERT(!s_BoxArtCacheDir.isEmpty());
    return s_BoxArtCacheDir;
}

QString Path::getQmlCacheDir()
{
    Q_ASSERT(!s_QmlCacheDir.isEmpty());
    return s_QmlCacheDir;
}

QString Path::getPortableRootDir()
{
    if (!s_PortableRootDir.isEmpty()) {
        return s_PortableRootDir;
    }

    return QDir(QCoreApplication::applicationDirPath()).absolutePath();
}

QByteArray Path::readDataFile(QString fileName)
{
    QFile dataFile(getDataFilePath(fileName));
    if (!dataFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    return dataFile.readAll();
}

void Path::writeCacheFile(QString fileName, QByteArray data)
{
    QDir cacheDir(s_CacheDir);

    // Create the cache path if it does not exist
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    QFile dataFile(cacheDir.absoluteFilePath(fileName));
    if (dataFile.open(QIODevice::WriteOnly)) {
        dataFile.write(data);
    }
}

void Path::deleteCacheFile(QString fileName)
{
    QFile dataFile(QDir(s_CacheDir).absoluteFilePath(fileName));
    dataFile.remove();
}

QFileInfo Path::getCacheFileInfo(QString fileName)
{
    return QFileInfo(QDir(s_CacheDir), fileName);
}

QString Path::getDataFilePath(QString fileName)
{
    QString candidatePath;

    // Check the cache location first (used by Path::writeDataFile())
    candidatePath = QDir(s_CacheDir).absoluteFilePath(fileName);
    if (QFile::exists(candidatePath)) {
        qInfo() << "Found" << fileName << "at" << candidatePath;
        return candidatePath;
    }

    // Check the app installation directory before the current directory.
    // This matters for portable builds launched via a shortcut or shell with
    // a different working directory.
    candidatePath = QDir(getPortableRootDir()).absoluteFilePath(fileName);
    if (QFile::exists(candidatePath)) {
        qInfo() << "Found" << fileName << "at" << candidatePath;
        return candidatePath;
    }

    // Check the current directory for developer/local overrides.
    candidatePath = QDir(QDir::currentPath()).absoluteFilePath(fileName);
    if (QFile::exists(candidatePath)) {
        qInfo() << "Found" << fileName << "at" << candidatePath;
        return candidatePath;
    }

    // Now check the data directories (for Linux, in particular)
    candidatePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, fileName);
    if (!candidatePath.isEmpty() && QFile::exists(candidatePath)) {
        qInfo() << "Found" << fileName << "at" << candidatePath;
        return candidatePath;
    }

    // Now try the directory of our app installation explicitly in case the
    // portable root was not initialized yet.
    candidatePath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(fileName);
    if (QFile::exists(candidatePath)) {
        qInfo() << "Found" << fileName << "at" << candidatePath;
        return candidatePath;
    }

    // Return the QRC embedded copy
    candidatePath = ":/data/" + fileName;
    qInfo() << "Found" << fileName << "at" << candidatePath;
    return QString(candidatePath);
}

void Path::initialize(bool portable, const QString& portableRootDir)
{
    if (portable) {
        s_PortableRootDir = QDir(portableRootDir.isEmpty() ?
                                 QCoreApplication::applicationDirPath() :
                                 portableRootDir).absolutePath();
        s_LogDir = QDir(s_PortableRootDir).filePath("logs");
        s_DumpDir = QDir(s_PortableRootDir).filePath("dumps");
        s_BoxArtCacheDir = s_PortableRootDir + "/boxart";
        s_QmlCacheDir = s_PortableRootDir + "/qmlcache";

        // In order for the If-Modified-Since logic to work in MappingFetcher,
        // the cache directory must be different than the current directory.
        s_CacheDir = s_PortableRootDir + "/cache";
    }
    else {
        s_PortableRootDir.clear();
        const QString appLocalDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        const QString appStoragePath = appLocalDataDir.isEmpty()
                ? QDir(QDir::tempPath()).filePath(QCoreApplication::applicationName())
                : appLocalDataDir;
        const QDir appStorageDir(appStoragePath);
        s_LogDir = appStorageDir.filePath("logs");
        s_DumpDir = appStorageDir.filePath("dumps");
        s_CacheDir = cacheDir;
        s_BoxArtCacheDir = cacheDir + "/boxart";
        s_QmlCacheDir = cacheDir + "/qmlcache";
    }
}
