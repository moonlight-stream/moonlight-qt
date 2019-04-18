#ifdef HAVE_I3IPC
#include <i3ipc++/ipc.hpp>
#include <i3ipc++/ipc-util.hpp>
#endif

#include <QThread>
#include <QDebug>

#include "i3windowmanager.h"

uint64_t find_focused_id(const i3ipc::container_t& container)
{
    if (container.focused && container.name.compare("Moonlight") == 0) {
        return container.id;
    }
    for (auto& c : container.nodes) {
        uint64_t ret = find_focused_id(*c);
        if (ret != 0) {
            return ret;
        }
    }
    return 0;
}
I3WindowManager::I3WindowManager()
    : isRunningI3(false)
{
}

void I3WindowManager::start()
{
#ifdef HAVE_I3IPC
    try {
        i3ipc::connection conn;
        appId = find_focused_id(*conn.get_tree());
        conn.send_command(QString("[con_id=%1] floating enable").arg(appId).toUtf8().constData());
        isRunningI3 = true;
    } catch (i3ipc::errno_error &err) {
        isRunningI3 = false;
    }
#endif
}

void I3WindowManager::hideWindow()
{
#ifdef HAVE_I3IPC
    if (!isRunningI3) {
        return;
    }
    try {
        i3ipc::connection conn;
        conn.send_command(QString("[con_id=%1] move scratchpad").arg(appId).toUtf8().constData());
    } catch (i3ipc::invalid_reply_payload_error &err) {
    }
#endif
}

void I3WindowManager::showWindow()
{
    if (!isRunningI3) {
        return;
    }
    try {
        i3ipc::connection conn;
        conn.send_command(QString("[con_id=%1] move scratchpad, scratchpad show").arg(appId).toUtf8().constData());
    } catch (i3ipc::invalid_reply_payload_error &err) {
    }
}
