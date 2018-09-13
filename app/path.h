#pragma once

#include <QString>

class Path
{
public:
    static QString getLogDir();

    static QString getBoxArtCacheDir();

    static QString getGamepadMappingFile();

    static void initialize(bool portable);

private:
    static QString s_LogDir;
    static QString s_BoxArtCacheDir;
    static const QString k_GameControllerMappingFile;
};
