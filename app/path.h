/**
 * @file app/path.h
 * @brief Path utility class for file and directory management.
 */

#pragma once

#include <QString>
#include <QFileInfo>

/**
 * @brief Utility class for managing application paths and file operations.
 */
class Path
{
public:
    static QString getLogDir();
    static QString getBoxArtCacheDir();
    static QString getQmlCacheDir();

    static QByteArray readDataFile(QString fileName);
    static void writeCacheFile(QString fileName, QByteArray data);
    static void deleteCacheFile(QString fileName);
    static QFileInfo getCacheFileInfo(QString fileName);

    // Only safe to use directly for Qt classes
    static QString getDataFilePath(QString fileName);

    static void initialize(bool portable);

private:
    static QString s_CacheDir;
    static QString s_LogDir;
    static QString s_BoxArtCacheDir;
    static QString s_QmlCacheDir;
};
