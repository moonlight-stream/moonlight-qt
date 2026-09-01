#pragma once

#include <QObject>

class QQuickWindow;
class Session;

// Windows-only global "boss key" (Ctrl+`) that hides and restores all Moonlight
// windows, including their taskbar buttons and Alt+Tab entries.
//
// This has to be a Win32 global hotkey rather than an SDL key combo for two
// reasons: SDL only sees keys while our window has focus (useless once we're
// hidden), and RegisterHotKey() swallows the keypress so it is never forwarded
// to the host PC while streaming.
class BossKey : public QObject
{
    Q_OBJECT

public:
    static BossKey* get();

    // Must be called from the main thread. Creates the message-only window that
    // receives WM_HOTKEY and registers the hotkey if the preference is enabled.
    void initialize(QQuickWindow* qtWindow);

    // Called by Session when a stream ends while we're hidden, so the launcher
    // window doesn't pop back into view.
    void notifySessionEndedWhileHidden(Session* session);

    // Invoked from our message-only window's WndProc
    void toggle();

    ~BossKey() override;

private slots:
    void onPreferenceChanged();

    void onSessionEndedWhileHidden();

private:
    explicit BossKey(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    void hideQtWindow();
    void showQtWindow();

    QQuickWindow* m_QtWindow;

    // Really an HWND. Kept opaque so we don't drag Windows.h into every
    // translation unit that needs to talk to the boss key.
    void* m_MessageWindow;

    bool m_Registered;
    bool m_Hidden;
    int m_SavedVisibility;
};
