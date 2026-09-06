#include "bosskey.h"

#include "settings/streamingpreferences.h"
#include "streaming/session.h"

#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>

#include "SDL_compat.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define BOSS_KEY_HOTKEY_ID 1
#define BOSS_KEY_WINDOW_CLASS L"MoonlightBossKeyWindow"

static LRESULT CALLBACK BossKeyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_HOTKEY && wParam == (WPARAM)BOSS_KEY_HOTKEY_ID) {
        auto bossKey = reinterpret_cast<BossKey*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (bossKey != nullptr) {
            bossKey->toggle();
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

BossKey* BossKey::get()
{
    static BossKey* s_BossKey = new BossKey(qApp);
    return s_BossKey;
}

BossKey::BossKey(QObject* parent)
    : QObject(parent),
      m_QtWindow(nullptr),
      m_MessageWindow(nullptr),
      m_Registered(false),
      m_Hidden(false),
      m_SavedVisibility(QWindow::Windowed)
{
}

BossKey::~BossKey()
{
    setEnabled(false);

    if (m_MessageWindow != nullptr) {
        DestroyWindow(reinterpret_cast<HWND>(m_MessageWindow));
        m_MessageWindow = nullptr;
    }
}

void BossKey::initialize(QQuickWindow* qtWindow)
{
    SDL_assert(m_MessageWindow == nullptr);

    m_QtWindow = qtWindow;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = BossKeyWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = BOSS_KEY_WINDOW_CLASS;

    // Ignore ERROR_CLASS_ALREADY_EXISTS, which we'll hit if we're reinitialized
    RegisterClassExW(&wc);

    // A message-only window created on the main thread is the key to making this
    // work in both phases of the app's life. Qt's event dispatcher pumps messages
    // during the launcher phase and SDL's WIN_PumpEvents pumps them during
    // Session::exec() (where Qt is suspended entirely), and both call
    // DispatchMessage() for every window on the thread. A thread-targeted hotkey
    // (RegisterHotKey(NULL, ...)) would not work, because DispatchMessage()
    // discards messages with a null HWND.
    HWND hwnd = CreateWindowExW(0, BOSS_KEY_WINDOW_CLASS, L"Moonlight Boss Key",
                                0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr);
    if (hwnd == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to create boss key message window: %d",
                     (int)GetLastError());
        return;
    }

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_MessageWindow = hwnd;

    StreamingPreferences* prefs = StreamingPreferences::get();
    connect(prefs, &StreamingPreferences::bossKeyEnabledChanged,
            this, &BossKey::onPreferenceChanged);

    setEnabled(prefs->bossKeyEnabled);
}

void BossKey::onPreferenceChanged()
{
    setEnabled(StreamingPreferences::get()->bossKeyEnabled);
}

void BossKey::setEnabled(bool enabled)
{
    if (m_MessageWindow == nullptr || enabled == m_Registered) {
        return;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_MessageWindow);

    if (enabled) {
        // VK_OEM_3 is the ` / ~ key on a US layout. MOD_NOREPEAT keeps a held key
        // from firing the hotkey over and over.
        if (!RegisterHotKey(hwnd, BOSS_KEY_HOTKEY_ID, MOD_CONTROL | MOD_NOREPEAT, VK_OEM_3)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to register boss key hotkey (Ctrl+`): %d. "
                        "It is probably already claimed by another application.",
                        (int)GetLastError());
            return;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Registered boss key hotkey (Ctrl+`)");
        m_Registered = true;
    }
    else {
        UnregisterHotKey(hwnd, BOSS_KEY_HOTKEY_ID);
        m_Registered = false;

        // Don't leave the user with no way to get their windows back
        if (m_Hidden) {
            toggle();
        }
    }
}

void BossKey::toggle()
{
    if (Session::get() != nullptr) {
        // We're streaming, so the session owns the hidden state of its own window.
        // Tracking it here too would let the two drift apart if the hotkey is hit
        // during the connection stages, before Session::exec() takes over.
        //
        // The Qt window is already hidden by StreamSegue.qml while streaming, so
        // there's nothing for us to do with it.
        m_Hidden = false;

        // We are re-entrant inside SDL_PumpEvents() right now, so let the session
        // do the work at a safe point in its own event loop.
        SDL_Event event = {};
        event.type = SDL_USEREVENT;
        event.user.timestamp = SDL_GetTicks();
        event.user.code = SDL_CODE_TOGGLE_BOSS_KEY;
        SDL_PushEvent(&event);
        return;
    }

    m_Hidden = !m_Hidden;

    if (m_Hidden) {
        hideQtWindow();
    }
    else {
        showQtWindow();
    }
}

void BossKey::hideQtWindow()
{
    if (m_QtWindow == nullptr || !m_QtWindow->isVisible()) {
        return;
    }

    // QWindow::hide() takes the taskbar button and Alt+Tab entry with it, so
    // there's nothing extra to do here.
    m_SavedVisibility = m_QtWindow->visibility();
    m_QtWindow->hide();
}

void BossKey::showQtWindow()
{
    if (m_QtWindow == nullptr) {
        return;
    }

    m_QtWindow->setVisibility(static_cast<QWindow::Visibility>(m_SavedVisibility));
    m_QtWindow->raise();

    // Windows grants foreground rights to the thread that received the last
    // hotkey input, so this won't be blocked as foreground stealing.
    m_QtWindow->requestActivate();
}

void BossKey::notifySessionEndedWhileHidden(Session* session)
{
    // Take ownership of the hidden state back from the session
    m_Hidden = true;

    // StreamSegue.qml makes the Qt window visible again when it handles this same
    // signal. Connecting now means our handler runs after QML's, and the extra
    // singleShot() hop guarantees we're last regardless of connection ordering.
    connect(session, &Session::sessionFinished,
            this, &BossKey::onSessionEndedWhileHidden,
            static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));
}

void BossKey::onSessionEndedWhileHidden()
{
    QTimer::singleShot(0, this, [this]() {
        if (m_Hidden) {
            // The window was made visible by QML behind our back, so we don't have a
            // meaningful saved visibility to capture here. Hide it and let the user's
            // next Ctrl+` restore it to a normal window.
            m_SavedVisibility = QWindow::Windowed;
            if (m_QtWindow != nullptr) {
                m_QtWindow->hide();
            }
        }
    });
}
