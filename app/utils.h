#pragma once

#include <QtGlobal>

#define THROW_BAD_ALLOC_IF_NULL(x) \
    if ((x) == nullptr) throw std::bad_alloc()

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define QNETREPLY_GET_ERROR(r) ((r)->networkError())
#define QSOCK_GET_ERROR(s) ((s)->socketError())
#else
#define QNETREPLY_GET_ERROR(r) ((r)->error())
#define QSOCK_GET_ERROR(s) ((s)->error())
#endif

namespace WMUtils {
    bool isRunningX11();
    bool isRunningWayland();
    bool isRunningWindowManager();
}
