#pragma once

#include <QTimer>
#include <QEvent>

#include "SDL_compat.h"

#include "settings/streamingpreferences.h"

class SdlGamepadKeyNavigation : public QObject
{
    Q_OBJECT

public:
    SdlGamepadKeyNavigation(StreamingPreferences* prefs);

    ~SdlGamepadKeyNavigation();

    Q_INVOKABLE void enable();

    Q_INVOKABLE void disable();

    Q_INVOKABLE void notifyWindowFocus(bool hasFocus);

    Q_INVOKABLE void setUiNavMode(bool settingsMode);

    // 临时挂起 UI 导航模式（下拉展开这类场景要拿回真正的方向键）。
    // 用计数而不是存旧值：挂起和恢复的配对由调用方保证，但和页面切换时的
    // setUiNavMode 谁先谁后不确定，存旧值会把页面刚设好的模式覆盖回去。
    Q_INVOKABLE void suspendUiNavMode();

    Q_INVOKABLE void resumeUiNavMode();

    Q_INVOKABLE int getConnectedGamepads();

private:
    // 实际生效的模式：页面要求开启，且当前没有被挂起
    bool uiNavModeActive() const;

    void sendKey(QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);

    void updateTimerState();

private slots:
    void onPollingTimerFired();

private:
    StreamingPreferences* m_Prefs;
    QTimer* m_PollingTimer;
    QList<SDL_GameController*> m_Gamepads;
    bool m_Enabled;
    bool m_UiNavMode;
    int m_UiNavSuspendCount;
    bool m_FirstPoll;
    bool m_HasFocus;
    Uint32 m_LastAxisNavigationEventTime;
};
