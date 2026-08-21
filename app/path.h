#pragma once

#include <QString>
#include <QFileInfo>

class Path
{
public:
    static QString getLogDir();
    static QString getDumpDir();
    static QString getBoxArtCacheDir();
    static QString getQmlCacheDir();
    static QString getPortableRootDir();

    static QByteArray readDataFile(QString fileName);
    static void writeCacheFile(QString fileName, QByteArray data);
    static void deleteCacheFile(QString fileName);
    static QFileInfo getCacheFileInfo(QString fileName);

    // Only safe to use directly for Qt classes
    static QString getDataFilePath(QString fileName);

    static void initialize(bool portable, const QString& portableRootDir = QString());

private:
    static QString s_CacheDir;
    static QString s_LogDir;
    static QString s_DumpDir;
    static QString s_BoxArtCacheDir;
    static QString s_QmlCacheDir;
    static QString s_PortableRootDir;
};
