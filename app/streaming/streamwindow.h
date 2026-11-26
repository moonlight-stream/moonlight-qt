#pragma once

#include <QString>
#include <qt_windows.h>

class QProcess;

// One detached GUI stream per host in this Windows logon session.
namespace StreamWindow {
inline constexpr auto CoverProperty = L"Moonlight.Display.Cover";
enum State { None, Opening, Connected };

QString outputKey(const QString& group, const QString& output);
State state(const QString& uuid, QString* progressText = nullptr);
bool isViewOnly(const QString& uuid);
bool activate(const QString& uuid);
bool launchOrActivate(QProcess& child, const QString& uuid);

void adoptReservation(const QString& uuid);
void setProgressWindow(HWND window);
void setConnectedWindow(HWND window);
void markReconnecting(bool retainWindow);
void release();
}
