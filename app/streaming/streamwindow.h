#pragma once

#include <QString>
#include <qt_windows.h>

class QProcess;

// One detached GUI stream per host in this Windows logon session.
namespace StreamWindow {
enum State { None, Opening, Connected };

State state(const QString& uuid, QString* progressText = nullptr);
bool isViewOnly(const QString& uuid);
bool activate(const QString& uuid);
bool launchOrActivate(QProcess& child, const QString& uuid);

void adoptReservation(const QString& uuid);
void setProgressWindow(HWND window);
void setConnectedWindow(HWND window);
void markReconnecting();
void release();
}
