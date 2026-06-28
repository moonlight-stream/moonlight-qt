#pragma once

#include <QSettings>

class NvApp
{
public:
    NvApp() {}
    explicit NvApp(QSettings& settings);

    bool operator==(const NvApp& other) const
    {
        return id == other.id &&
                name == other.name &&
                hdrSupported == other.hdrSupported &&
                isAppCollectorGame == other.isAppCollectorGame &&
                hidden == other.hidden &&
                directLaunch == other.directLaunch;
    }

    bool operator!=(const NvApp& other) const
    {
        return !operator==(other);
    }

    bool isInitialized()
    {
        // We use isNull() instead of isEmpty() here because we want
        // to detect cases where the name is unassigned, not empty.
        return id != 0 && !name.isNull();
    }

    void
    serialize(QSettings& settings) const;

    int id = 0;
    QString name;
    bool hdrSupported = false;
    bool isAppCollectorGame = false;
    bool hidden = false;
    bool directLaunch = false;
};

Q_DECLARE_METATYPE(NvApp)
