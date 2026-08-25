#pragma once

#include <QString>

namespace DeviceLocalSettings {

/**
 * Returns a writable path for state that belongs to this device and must not
 * be included in Moonlight's normal or portable configuration.
 */
QString filePath(const QString& fileName, const char* featureOverrideVariable = nullptr);

}
