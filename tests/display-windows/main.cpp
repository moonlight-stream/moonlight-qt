#include "displaywindows.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <cstdio>
#include <algorithm>
#include <atomic>
#include <vector>

namespace {
constexpr UINT SelectMain = WM_APP + 1;
constexpr UINT DragSelf = WM_APP + 2;
constexpr UINT MoveSelf = WM_APP + 3;
constexpr UINT DropSelf = WM_APP + 4;
constexpr UINT HoldFrames = WM_APP + 5;
constexpr UINT RenderFrame = WM_APP + 6;
constexpr UINT Dock = WM_APP + 7;
constexpr UINT StallMoves = WM_APP + 8;
constexpr UINT ReadMoveCount = WM_APP + 9;
constexpr UINT DragBack = WM_APP + 10;
constexpr auto MoveGate = L"Moonlight.Test.MoveGate";
constexpr auto HoldThird = L"Moonlight.Test.HoldThird";
constexpr UINT FrameTimer = 2;
DisplayWindows* tabs = nullptr;
bool holdFrames = false;
bool rootShownDuringClose = false;
bool closing = false;
int moveCount = 0;

RECT bounds(HWND window)
{
    RECT rect{};
    GetWindowRect(window, &rect);
    return rect;
}

bool onScreen(HWND window)
{
    DWORD cloaked = 0;
    DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    return IsWindowVisible(window) && !cloaked;
}

LRESULT CALLBACK procedure(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case DisplayWindows::ReadyMessage: tabs->tick(); return 0;
    case DisplayWindows::SwitchMessage: tabs->prepareSwitch(reinterpret_cast<HWND>(wp)); return 0;
    case DisplayWindows::SwitchedMessage: tabs->switched(reinterpret_cast<HWND>(wp)); return 0;
    case DisplayWindows::LayoutMessage: tabs->dragWindowPositioned(); return 0;
    case DisplayWindows::DragMoveFenceMessage:
        tabs->acknowledgeDragMove(reinterpret_cast<HWND>(lp), wp);
        return 0;
    case DisplayWindows::DragMoveAppliedMessage: tabs->dragMoveApplied(wp); return 0;
    case StallMoves: {
        moveCount = 0;
        SetPropW(hwnd, MoveGate, reinterpret_cast<HANDLE>(1));
        QElapsedTimer wait;
        wait.start();
        while (GetPropW(hwnd, MoveGate) && wait.elapsed() < 5000) Sleep(1);
        if (GetPropW(hwnd, MoveGate)) moveCount = 1000; // The sender blocked waiting for us.
        return 0;
    }
    case ReadMoveCount: return moveCount;
    case SelectMain: tabs->select("A"); return 0;
    case DragSelf: tabs->beginDrag("B", {"A", "B"}, {218, 118}, {480, 510}); return 0;
    case MoveSelf: tabs->moveDrag({510, 530}); return 0;
    case DropSelf: tabs->finishDrag({522, 540}); return 0;
    case DragBack: {
        const RECT rect = bounds(hwnd);
        const POINT grip{rect.left + 100, rect.top + 16};
        const POINT drop{static_cast<LONG>(wp), static_cast<LONG>(lp)};
        if (!tabs->beginDrag("B", {"A", "B"}, grip, drop)) return 0;
        tabs->finishDrag(drop);
        return 0;
    }
    case Dock: tabs->dock(); return 0;
    case HoldFrames: holdFrames = wp != 0; return 0;
    case RenderFrame: tabs->framePresented(); return 0;
    case WM_TIMER:
        if (wp == FrameTimer) {
            KillTimer(hwnd, FrameTimer);
            if (!holdFrames) tabs->framePresented();
        } else tabs->tick();
        return 0;
    case WM_CLOSE:
        if (tabs->canDock()) { tabs->dock(); return 0; }
        tabs->endGroup();
        tabs->close();
        break;
    case WM_WINDOWPOSCHANGED:
        if (!(reinterpret_cast<WINDOWPOS*>(lp)->flags & SWP_NOMOVE)) ++moveCount;
        if (closing && (reinterpret_cast<WINDOWPOS*>(lp)->flags & SWP_SHOWWINDOW)) rootShownDuringClose = true;
        break;
    case WM_DESTROY: PostQuitMessage(0); break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool waitFor(const std::function<bool()>& ready)
{
    QElapsedTimer clock;
    clock.start();
    while (!ready() && clock.elapsed() < 5000) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        tabs->tick();
        if (!ready()) MsgWaitForMultipleObjectsEx(0, nullptr, 5, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    return ready();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const bool child = app.arguments().contains("--peer");
    WNDCLASSW wc{};
    wc.lpfnWndProc = procedure;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MoonlightDisplayWindowTest";
    RegisterClassW(&wc);
    const auto output = child ? qEnvironmentVariable("MOONLIGHT_CAPTURE_OUTPUT") : QStringLiteral("A");
    const auto id = qEnvironmentVariable("MOONLIGHT_DISPLAY_GROUP");
    const auto title = child ? id + output : QStringLiteral("Main display window test");
    const auto window = CreateWindowW(wc.lpszClassName, title.toStdWString().c_str(), WS_OVERLAPPEDWINDOW,
                                      100, 100, 240, 180, nullptr, nullptr, wc.hInstance, nullptr);
    if (!window) return 1;
    if (child) StreamWindow::adoptReservation(StreamWindow::outputKey(id, output));
    std::atomic<int> launches{0};
    std::atomic<int> attempts{0};
    std::atomic<int> explicitAttempt{0}, queuedAttempt{0};
    std::promise<QString> firstGroup;
    std::promise<void> allowLaunch;
    auto launchAllowed = allowLaunch.get_future().share();
    const QString outputList = QStringLiteral(R"([{"id":"A"},{"id":"B"},{"id":"C"},{"id":"E"}])");
    DisplayWindows group([&](const QString& output, const QString& id, quintptr root, const QString& snapshot) {
        if (child) return false;
        if (snapshot != outputList) std::abort();
        const int attempt = ++attempts;
        if (attempt == 1) {
            firstGroup.set_value(id);
            launchAllowed.wait();
            return false; // The first drag must retry a failed background prewarm.
        }
        if (output == "D") { explicitAttempt = attempt; return false; }
        if (output == "E") { queuedAttempt = attempt; return false; }
        ++launches;
        QProcess peer;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("MOONLIGHT_CAPTURE_OUTPUT", output);
        env.insert("MOONLIGHT_DISPLAY_GROUP", id);
        env.insert("MOONLIGHT_DISPLAY_ROOT", QString::number(root));
        peer.setProcessEnvironment(env);
        peer.setProgram(app.applicationFilePath());
        peer.setArguments({"--peer"});
        return StreamWindow::launchOrActivate(peer, StreamWindow::outputKey(id, output));
    }, [&](bool) { SetTimer(window, FrameTimer, 16, nullptr); });
    tabs = &group;
    holdFrames = output == "B";
    if (child && output == "C") {
        const auto root = reinterpret_cast<HWND>(quintptr(qgetenv("MOONLIGHT_DISPLAY_ROOT").toULongLong()));
        QElapsedTimer clock;
        clock.start();
        while (GetPropW(root, HoldThird) && clock.elapsed() < 5000) Sleep(1);
    }
    group.bind(window, output);
    if (child) {
        StreamWindow::setConnectedWindow(window);
        SetTimer(window, 1, 30, nullptr);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) DispatchMessageW(&msg);
        StreamWindow::release();
        return 0;
    }
    ShowWindow(window, SW_SHOWNOACTIVATE);
    SetPropW(window, HoldThird, reinterpret_cast<HANDLE>(1));
    HWND secondary = nullptr;
    group.prewarm({"A", "B", "C", "E"}, outputList);
    group.prewarm({"A", "B", "C", "E"}, outputList);
    const auto groupName = firstGroup.get_future().get();
    // The launcher is deliberately blocked. A full-size drag surface must
    // already follow the grip, including the last position after button-up.
    RECT preview{};
    const auto previewAt = [&](LONG x, LONG y) {
        return group.previewBounds(nullptr, preview) && preview.left == x && preview.top == y &&
               preview.right - preview.left == 240 && preview.bottom - preview.top == 180;
    };
    bool followed = group.beginDrag("B", {"A", "B"}, {200, 116}, {400, 380}) && previewAt(300, 364);
    group.moveDrag({410, 400});
    followed &= previewAt(310, 384);
    group.finishDrag({420, 410});
    followed &= previewAt(320, 394);
    allowLaunch.set_value();
    if (!followed) return 3;
    if (!waitFor([&] {
        secondary = FindWindowW(wc.lpszClassName, (groupName + "B").toStdWString().c_str());
        return secondary && !onScreen(secondary) && onScreen(window);
    }) || launches != 1) return 2;

    PostMessageW(secondary, RenderFrame, 0, 0);
    if (!waitFor([&] {
        RECT rect = bounds(secondary);
        return onScreen(window) && onScreen(secondary) && rect.left == 320 && rect.top == 394 &&
               !group.previewBounds(nullptr, preview);
    })) return 4;
    PostMessageW(secondary, Dock, 0, 0);
    if (!waitFor([&] { return !IsWindowVisible(secondary); })) return 5;

    // The next output warms in the background without a click. Repeated
    // discovery must keep both ready companions hidden.
    if (!waitFor([&] { return StreamWindow::state(StreamWindow::outputKey(groupName, "C")) == StreamWindow::Opening; }))
        return 46;
    if (!group.select("D")) return 47;
    group.prewarm({"A", "B", "C", "E"}, outputList);
    // Keep C's reservation in Opening after CreateProcess completes.
    QElapsedTimer held;
    held.start();
    if (!waitFor([&] { return held.elapsed() >= 100; }) || attempts != 3) return 48;
    RemovePropW(window, HoldThird);
    HWND third = nullptr;
    if (!waitFor([&] {
        third = FindWindowW(wc.lpszClassName, (groupName + "C").toStdWString().c_str());
        return third && GetPropW(third, L"Moonlight.Display.HasFrame") && !IsWindowVisible(third);
    })) return 44;
    if (!waitFor([&] { return queuedAttempt != 0; }) || explicitAttempt != 4 || queuedAttempt != 5) return 49;
    group.prewarm({"A", "B", "C", "E"}, outputList);
    if (launches != 2 || attempts != 5 || IsWindowVisible(secondary)) return 45;

    // Replay partial successful polls with B streaming in a hidden window.
    const QJsonObject a{{"id", "A"}}, b{{"id", "B"}}, gone{{"id", "unplugged"}};
    const QJsonArray both{a, b};
    if (group.selectableOutputs({a}, {a, b, gone}) != both ||
        group.selectableOutputs({b}, both) != both ||
        group.selectableOutputs({}, both) != both) return 50;

    if (!group.select("B") || group.pendingTab() != "B") return 6;
    auto staged = [&] {
        DWORD cloaked = 0;
        DwmGetWindowAttribute(secondary, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
        return IsWindowVisible(secondary) && (cloaked & DWM_CLOAKED_APP) && IsWindowVisible(window);
    };
    if (!waitFor(staged)) return 7;
    // A later click on the original tab wins, even if the stale frame arrives.
    if (!group.select("A")) return 8;
    PostMessageW(secondary, RenderFrame, 0, 0);
    if (!waitFor([&] { return IsWindowVisible(window) && !IsWindowVisible(secondary); })) return 9;
    // A stalled handoff expires. A late presentation cannot steal focus.
    if (!group.select("B") || !waitFor(staged) || group.tick(GetTickCount64() + 6000) ||
        !group.pendingTab().isEmpty()) return 35;
    PostMessageW(secondary, RenderFrame, 0, 0);
    if (!waitFor([&] { return onScreen(window) && !IsWindowVisible(secondary); })) return 36;
    if (!group.select("B") || !waitFor(staged)) return 10;
    PostMessageW(secondary, HoldFrames, 0, 0);
    PostMessageW(secondary, RenderFrame, 0, 0);
    if (!waitFor([&] { return IsWindowVisible(secondary) && !IsWindowVisible(window); })) return 11;

    std::vector<double> elapsed;
    for (int cycle = 0; cycle < 25; ++cycle) {
        QElapsedTimer timer;
        timer.start();
        PostMessageW(secondary, SelectMain, 0, 0);
        if (!waitFor([&] { return IsWindowVisible(window) && !IsWindowVisible(secondary); })) return 12;
        elapsed.push_back(timer.nsecsElapsed() / 1e6);
        timer.restart();
        if (!group.select("B")) return 13;
        if (!waitFor([&] { return !IsWindowVisible(window) && IsWindowVisible(secondary); })) return 14;
        elapsed.push_back(timer.nsecsElapsed() / 1e6);
    }
    std::sort(elapsed.begin(), elapsed.end());
    std::printf("50 warm switches with a simulated render callback, p95 %.1f ms\n", elapsed[47]);

    PostMessageW(secondary, DragSelf, 0, 0);
    auto secondAt = [&](LONG x, LONG y) {
        RECT rect = bounds(secondary);
        return IsWindowVisible(window) && IsWindowVisible(secondary) && rect.left == x && rect.top == y;
    };
    if (!waitFor([&] { return secondAt(362, 492); })) return 15;
    PostMessageW(secondary, MoveSelf, 0, 0);
    if (!waitFor([&] { return secondAt(392, 512); })) return 16;
    PostMessageW(secondary, DropSelf, 0, 0);
    if (!waitFor([&] { return secondAt(404, 522); })) return 17;
    PostMessageW(secondary, Dock, 0, 0);
    if (!waitFor([&] { return !IsWindowVisible(secondary) && bounds(window).left == 100; })) return 18;

    // Drag the inactive tab while both processes are already warm.
    if (!group.beginDrag("B", {"A", "B"}, {300, 120}, {500, 450})) return 19;
    if (!waitFor([&] { return secondAt(300, 430); })) return 20;
    // Stall the target HWND while the source receives a fast burst of mouse positions.
    const auto queueBurst = [&] {
        PostMessageW(secondary, StallMoves, 0, 0);
        if (!waitFor([&] { return GetPropW(secondary, MoveGate) != nullptr; })) return false;
        for (int i = 1; i <= 256; ++i) group.moveDrag({500 + i, 450});
        return true;
    };
    const auto moveCountInRange = [&] {
        const auto count = SendMessageW(secondary, ReadMoveCount, 0, 0);
        std::printf("256 mouse moves on a busy peer produced %lld window moves\n", static_cast<long long>(count));
        return count >= 1 && count <= 3;
    };
    if (!queueBurst()) return 37;
    RemovePropW(secondary, MoveGate);
    if (!waitFor([&] { return secondAt(556, 430); }) || !moveCountInRange()) return 38;
    if (!queueBurst()) return 39;
    group.finishDrag({530, 480});
    RemovePropW(secondary, MoveGate);
    if (!waitFor([&] { return secondAt(330, 460); })) return 21;
    if (!moveCountInRange()) return 40;
    const RECT normal = bounds(window);
    // Drop the floating tab back on the real tab strip of the shared window.
    const HWND strip = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, 20, 8, 140, 24,
                                     window, nullptr, wc.hInstance, nullptr);
    if (!strip) return 50;
    DisplayWindows::setTabBar(window, strip);
    const RECT bar = bounds(strip);
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE);
    PostMessageW(secondary, DragBack, (bar.left + bar.right) / 2, (bar.top + bar.bottom) / 2);
    if (!waitFor([&] {
        const RECT returned = bounds(secondary);
        return onScreen(secondary) && !IsWindowVisible(window) && EqualRect(&normal, &returned) &&
               !(GetWindowLongPtrW(secondary, GWL_EXSTYLE) & WS_EX_TOPMOST);
    })) return 51;
    PostMessageW(secondary, SelectMain, 0, 0);
    if (!waitFor([&] { return onScreen(window) && !IsWindowVisible(secondary); })) return 22;
    DisplayWindows::setTabBar(window, nullptr);
    DestroyWindow(strip);
    ShowWindow(window, SW_MAXIMIZE);
    const RECT maximized = bounds(window);
    if (!group.select("B") || !waitFor([&] { return !IsWindowVisible(window) && IsZoomed(secondary); })) return 26;
    RECT rect = bounds(secondary);
    if (!EqualRect(&rect, &maximized)) return 27;
    PostMessageW(secondary, SelectMain, 0, 0);
    if (!waitFor([&] { return IsZoomed(window) && IsWindowVisible(window) && !IsWindowVisible(secondary); })) return 28;
    ShowWindow(window, SW_RESTORE);
    rect = bounds(window);
    if (!EqualRect(&rect, &normal)) return 29;
    if (launches != 2 || !group.beginDrag("C", {"A", "B", "C"}, {270, 119}, {610, 424})) return 30;
    group.finishDrag({630, 460});
    if (!waitFor([&] {
        third = FindWindowW(wc.lpszClassName, (groupName + "C").toStdWString().c_str());
        const RECT dropped = bounds(third);
        return third && onScreen(third) && onScreen(window) && !group.previewBounds(nullptr, preview) &&
               dropped.left == normal.left + 630 - 270 && dropped.top == normal.top + 460 - 119;
    })) return 31;
    const RECT floating = bounds(third);
    // Pull the main tab with the first alternative already in its own window.
    if (!group.beginDrag("A", {"C", "A", "B"}, {230, 118}, {400, 270})) return 32;
    group.finishDrag({410, 280});
    if (!waitFor([&] { return group.canDock() && IsWindowVisible(secondary); })) return 33;
    rect = bounds(third);
    if (!IsWindowVisible(third) || !EqualRect(&rect, &floating)) return 34;
    group.dock();
    if (IsWindowVisible(window) || !IsWindowVisible(secondary) || launches != 2) return 23;
    if (!waitFor([&] { return IsWindowVisible(secondary) && !IsWindowVisible(window); })) return 24;
    closing = true;
    PostMessageW(secondary, WM_CLOSE, 0, 0);
    if (!waitFor([&] {
        return !IsWindow(window) && !IsWindow(third) &&
               StreamWindow::state(StreamWindow::outputKey(groupName, "B")) == StreamWindow::None &&
               StreamWindow::state(StreamWindow::outputKey(groupName, "C")) == StreamWindow::None;
    }) || rootShownDuringClose) return 25;
    std::puts("PASS: cold/warm drag, frame-gated switching, cancellation, maximize, dock and one-click three-display close");
    return 0;
}
