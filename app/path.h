#pragma once

#include <QString>

class Path
{
public:
    static QString getLogDir();

    static QString getBoxArtCacheDir();

    static QByteArray readDataFile(QString fileName);
    static void writeDataFile(QString fileName, QByteArray data);

    // Only safe to use directly for Qt classes
    static QString getDataFilePath(QString fileName);

    static void initialize(bool portable);

private:
    static QString s_CacheDir;
    static QString s_LogDir;
    static QString s_BoxArtCacheDir;
};
