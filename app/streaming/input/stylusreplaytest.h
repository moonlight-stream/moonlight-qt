#pragma once

#include <QRect>
#include <QString>

#include <functional>
#include <memory>

struct SDL_Window;

/**
 * Developer-only stylus replay harness.
 *
 * This class is the boundary for all test-only runtime behavior: the control
 * panel, file picker, parser, replay scheduler, local mouse filter state, and
 * protocol cleanup. It is compiled only with MOONLIGHT_ENABLE_FUNCTION_TESTS.
 * Keep production streaming behavior out of this class and keep Session's
 * integration limited to opening, ticking, waiting, filtering, and teardown.
 */
class StylusReplayTest final
{
public:
    using ToastCallback = std::function<void(const QString&, int)>;
    using PanelClosedCallback = std::function<void()>;

    StylusReplayTest(SDL_Window* streamingWindow,
                     ToastCallback toastCallback,
                     PanelClosedCallback panelClosedCallback);
    ~StylusReplayTest();

    void requestShow(const QRect& parentGeometry);
    void closePanel();
    bool isPanelVisible() const;

    void process();
    int nextDelayMs() const;
    void stop(bool notifyUser);

    bool shouldFilterLocalMouseInput() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};
