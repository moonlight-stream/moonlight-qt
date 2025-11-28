/**
 * @file app/utils.h
 * @brief Utility functions and macros.
 */

#pragma once

#include <QString>

/**
 * @def THROW_BAD_ALLOC_IF_NULL
 * @brief Macro to throw std::bad_alloc if pointer is null.
 */
#define THROW_BAD_ALLOC_IF_NULL(x) \
    if ((x) == nullptr) throw std::bad_alloc()

/**
 * @brief Window manager utility functions.
 */
namespace WMUtils {
    bool isRunningX11();
    bool isRunningWayland();
    bool isRunningWindowManager();
    bool isRunningDesktopEnvironment();
    QString getDrmCardOverride();
}
