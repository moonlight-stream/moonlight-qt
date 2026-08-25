#include "devicelocalsettings.h"

#include <QDir>
#include <QStandardPaths>

namespace {
QString stateDirectory(const char* featureOverrideVariable)
{
    if (featureOverrideVariable && *featureOverrideVariable) {
        const QString featureOverride = qEnvironmentVariable(featureOverrideVariable);
        if (!featureOverride.isEmpty()) {
            return QDir::cleanPath(featureOverride);
        }
    }

    const QString sharedOverride = qEnvironmentVariable(
            "MOONLIGHT_DEVICE_LOCAL_SETTINGS_DIR");
    if (!sharedOverride.isEmpty()) {
        return QDir::cleanPath(sharedOverride);
    }

    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}
}

QString DeviceLocalSettings::filePath(const QString& fileName,
                                      const char* featureOverrideVariable)
{
    QDir directory(stateDirectory(featureOverrideVariable));
    if (!directory.path().isEmpty() &&
            (directory.exists() || directory.mkpath(QStringLiteral(".")))) {
        return directory.filePath(fileName);
    }

    QDir fallbackDirectory(QDir::temp().filePath(QStringLiteral("Moonlight")));
    fallbackDirectory.mkpath(QStringLiteral("."));
    qWarning("Failed to create the device-local settings directory; using temporary storage");
    return fallbackDirectory.filePath(fileName);
}
