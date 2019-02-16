#pragma once

#include <QString>

class Path
{
public:
    static QString getLogDir();

    static QString getBoxArtCacheDir();

    static QString getDataFilePath(QString fileName);

    static void initialize(bool portable);

private:
    static QString s_LogDir;
    static QString s_BoxArtCacheDir;
};
