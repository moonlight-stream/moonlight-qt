#pragma once

#include <QString>

class Path
{
public:
    static QString getLogDir();

    static QString getBoxArtCacheDir();

    static void initialize(bool portable);

    static QString s_LogDir;
    static QString s_BoxArtCacheDir;
};
