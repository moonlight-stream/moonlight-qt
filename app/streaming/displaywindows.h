#pragma once

#include "streamwindow.h"
#include "windowcaption.h"

#include <QSet>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QUuid>
#include <algorithm>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <utility>

// Each output keeps its own decoder and input HWND. The root records which one
// occupies the shared tabbed window, independently of which stream owns the root.
class DisplayWindows {
public:
    using Launch = std::function<bool(const QString&, const QString&, quintptr, const QString&)>;

    DisplayWindows(Launch launch, std::function<void(bool)> requestFrame)
        : m_Launch(std::move(launch))
        , m_RequestFrame(std::move(requestFrame))
        , m_Output(qEnvironmentVariable("MOONLIGHT_CAPTURE_OUTPUT"))
        , m_Group(m_Output.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) :
                                     qEnvironmentVariable("MOONLIGHT_DISPLAY_GROUP"))
        , m_GroupProperty((QStringLiteral("Moonlight.Display.") + m_Group).toStdWString())
        , m_Root(reinterpret_cast<HWND>(quintptr(qgetenv("MOONLIGHT_DISPLAY_ROOT").toULongLong())))
    {}

    const QString& output() const { return m_Output; }
    QJsonArray selectableOutputs(QJsonArray reported, const QJsonArray& previous) const
    {
        QSet<QString> listed;
        for (const auto& entry : reported)
            listed.insert(entry.toObject().value("id").toString());
        // Host discovery can omit a display while its independent stream is still open.
        for (qsizetype i = 0; i < previous.size(); ++i) {
            const auto entry = previous.at(i);
            const QString id = entry.toObject().value("id").toString();
            if (id.isEmpty() || listed.contains(id)) continue;
            if (find(id) || StreamWindow::state(StreamWindow::outputKey(m_Group, id)) == StreamWindow::Opening) {
                reported.insert(std::min(i, reported.size()), entry);
                listed.insert(id);
            }
        }
        return reported;
    }
    bool hasFocus() const { return member(GetForegroundWindow()); }
    HWND groupedWindow() const
    {
        const HWND front = static_cast<HWND>(GetPropW(m_Root, StreamWindow::CoverProperty));
        return member(front) ? front : m_Root;
    }
    bool canDock() const { return member(m_Root) && GetPropW(m_Window, Detached) && !ending(); }

    static void setTabBar(HWND owner, HWND tabs)
    {
        if (tabs) SetPropW(owner, TabBar, tabs);
        else if (owner) RemovePropW(owner, TabBar);
    }

    void dock()
    {
        if (!canDock()) return;
        HWND front = groupedWindow();
        cancelPending();
        RemovePropW(m_Window, Detached);
        if (IsIconic(front)) ShowWindowAsync(front, SW_RESTORE);
        SetForegroundWindow(front);
        ShowWindow(m_Window, SW_HIDE);
    }

    void prewarm(const QStringList& outputs, const QString& outputList)
    {
        m_OutputList = outputList;
        if (!m_Output.isEmpty() || !m_Window || !m_Launch || ending()) return;
        m_PrewarmQueue.clear();
        for (const auto& output : outputs) {
            if (!output.isEmpty() && output != m_BoundOutput && !m_AttemptedOutputs.contains(output))
                m_PrewarmQueue.append(output);
        }
        prewarmNext();
    }

    void bind(HWND window, const QString& output)
    {
        if (!window || output.isEmpty() || (m_Window == window && output == m_BoundOutput)) return;
        if (m_Window != window) {
            close();
            m_Window = window;
            if (m_Output.isEmpty()) m_Root = window;
            SetPropW(window, m_GroupProperty.c_str(), reinterpret_cast<HANDLE>(1));
        } else {
            RemovePropW(window, m_OutputProperty.c_str());
            RemovePropW(window, HasFrame);
        }
        m_BoundOutput = output;
        m_OutputProperty = property(output);
        // Publish the HWND before its first frame. A click can now stage the
        // window and drive rendering even on a backend that skips hidden HWNDs.
        SetPropW(window, m_OutputProperty.c_str(), reinterpret_cast<HANDLE>(1));
        if (window != m_Root && member(m_Root) && !IsWindowVisible(window)) {
            RECT bounds{};
            const HWND front = groupedWindow();
            if (!IsIconic(front) && GetWindowRect(front, &bounds)) {
                m_Warming = cloak(true);
                SetWindowPos(window, front, bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top,
                             SWP_NOACTIVATE | (m_Warming ? SWP_SHOWWINDOW : 0));
            }
        }
        requestFrame();
        notifyRequester();
    }

    // Called on the window thread after the renderer has submitted a frame.
    void framePresented()
    {
        if (!m_Window || ending()) return;
        m_KeyframeAt = 0;
        if (std::exchange(m_Warming, false) && !m_StagingBehind) {
            ShowWindow(m_Window, SW_HIDE);
            cloak(false);
        }
        if (!GetPropW(m_Window, HasFrame)) {
            SetPropW(m_Window, HasFrame, reinterpret_cast<HANDLE>(1));
            notifyRequester();
        }
        if (!m_StagingBehind) return;
        const HWND previous = m_StagingBehind;
        if (!switchRequested(previous)) {
            cancelStaging();
            return;
        }

        m_StagingBehind = nullptr;
        const bool split = GetPropW(m_Root, PullingActive) == previous;
        const bool focus = !split && hasFocus();
        if (m_Window == m_Root) RemovePropW(m_Root, StreamWindow::CoverProperty);
        else SetPropW(m_Root, StreamWindow::CoverProperty, m_Window);
        if (focus) SetWindowPos(m_Window, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        cloak(false);
        if (focus) SetForegroundWindow(m_Window);
        // Keep the previous frame on screen until the compositor has processed
        // the reveal. The previous window also checks the latest tab selection.
        DwmFlush();
        PostMessageW(previous, SwitchedMessage, reinterpret_cast<WPARAM>(m_Window), 0);
    }

    void prepareSwitch(HWND previous)
    {
        if (!switchRequested(previous) || m_StagingBehind == previous) return;
        m_StagingBehind = previous;
        // Showing and sizing an uncloaked HWND exposes its empty swap chain and
        // may animate the window. Prepare its video and owned caption together.
        cloak(true);
        RECT rect{};
        GetWindowRect(previous, &rect);
        WINDOWPLACEMENT placement{sizeof(placement)};
        UINT flags = SWP_NOACTIVATE;
        if (IsZoomed(previous) && GetWindowPlacement(previous, &placement)) {
            // Copy the restore bounds as well as the maximized state. Applying
            // an explicit size afterwards would turn it into a normal window.
            placement.showCmd = SW_SHOWMAXIMIZED;
            SetWindowPlacement(m_Window, &placement);
            ShowWindow(m_Window, SW_SHOWMAXIMIZED);
            flags |= SWP_NOMOVE | SWP_NOSIZE;
        } else {
            if (IsZoomed(m_Window) || IsIconic(m_Window)) ShowWindow(m_Window, SW_SHOWNOACTIVATE);
            flags |= SWP_SHOWWINDOW;
        }
        SetWindowPos(m_Window, previous, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                     flags);
        requestFrame();
    }

    void switched(HWND front)
    {
        if (!member(front) || front != groupedWindow() || m_Window == front || ending()) return;
        if (m_Drag && m_Drag->pullSelf && ownsRequest()) {
            startMoving(m_Window);
        } else if (!GetPropW(m_Window, Detached) && !m_StagingBehind) {
            // The new process may have lost its foreground grant while waiting
            // for a frame. The old foreground process can still hand focus over.
            if (GetForegroundWindow() == m_Window) SetForegroundWindow(front);
            ShowWindow(m_Window, SW_HIDE);
        }
    }

    // Only an explicit close of the tabbed window ends the group. A failed
    // auxiliary stream can still disappear without disconnecting the main one.
    void endGroup(bool disconnectOnly = false)
    {
        if (!member(m_Root) || ending()) return;
        SetPropW(m_Root, Closing, reinterpret_cast<HANDLE>(1));
        struct ClosingGroup { DisplayWindows* group; UINT message; } data{this, disconnectOnly ? DisconnectMessage : WM_CLOSE};
        EnumWindows([](HWND hwnd, LPARAM arg) -> BOOL {
            const auto& data = *reinterpret_cast<ClosingGroup*>(arg);
            if (hwnd != data.group->m_Window && data.group->member(hwnd)) PostMessageW(hwnd, data.message, 0, 0);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&data));
    }

    void close()
    {
        if (!m_Window) return;
        cancelPending();
        if (m_StagingBehind) cancelStaging();
        RemovePropW(m_Window, Detached);
        RemovePropW(m_Window, HasFrame);
        if (std::exchange(m_Warming, false)) {
            ShowWindow(m_Window, SW_HIDE);
            cloak(false);
        }
        m_KeyframeAt = 0;
        RemovePropW(m_Window, m_OutputProperty.c_str());
        RemovePropW(m_Window, m_GroupProperty.c_str());
        if (m_Window == m_Root) {
            RemovePropW(m_Root, StreamWindow::CoverProperty);
            RemovePropW(m_Root, Closing);
            RemovePropW(m_Root, RequestOwner);
            RemovePropW(m_Root, SwitchTarget);
            RemovePropW(m_Root, PullingActive);
        }
        m_Window = nullptr;
        m_BoundOutput.clear();
    }

    bool tick(ULONGLONG now = GetTickCount64())
    {
        if (!m_Window) return true;
        if (!member(m_Root)) {
            PostMessageW(m_Window, WM_CLOSE, 0, 0);
            return true;
        }
        if (ending()) return true;
        if (!collectLaunch()) return false;
        if (m_KeyframeAt && now >= m_KeyframeAt) {
            m_KeyframeAt = 0;
            m_RequestFrame(true);
        }
        if (m_StagingBehind && !switchRequested(m_StagingBehind)) cancelStaging();
        if ((m_Drag || !m_PendingOutput.isEmpty()) && !ownsRequest()) cancelPending();
        if (m_Drag && m_Drag->awaitingPlacement) {
            if (!member(m_Drag->moving) || now >= m_Deadline) {
                cancelPending();
                return false;
            }
            if (IsWindowVisible(m_Drag->moving)) {
                m_Drag->awaitingPlacement = false;
                if (m_Drag->released) activateDrop();
            }
        }
        // A peer can publish its newly presented frame between property reads.
        // Use the same cover snapshot for both lookup and recovery.
        const HWND cover = static_cast<HWND>(GetPropW(m_Root, StreamWindow::CoverProperty));
        const HWND front = member(cover) ? cover : m_Root;
        if (m_Window == m_Root && cover && front == m_Root) {
            // Recover the shared window if its foreground stream died unexpectedly.
            RemovePropW(m_Root, StreamWindow::CoverProperty);
            RemovePropW(m_Window, Detached);
            ShowWindow(m_Window, SW_SHOWNA);
        }
        if (!IsWindowVisible(m_Window) && IsWindowVisible(front) && !IsIconic(front)) {
            RECT rect{}, current{};
            if (GetWindowRect(front, &rect) && GetWindowRect(m_Window, &current) && !EqualRect(&current, &rect))
                SetWindowPos(m_Window, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
        }
        if (m_PendingOutput.isEmpty()) {
            prewarmNext();
            return true;
        }
        const HWND switching = static_cast<HWND>(GetPropW(m_Root, SwitchTarget));
        if (switching && member(switching) && switching == front && !m_Drag) {
            m_PendingOutput.clear();
            return true;
        }
        if (now >= m_Deadline || (switching && !member(switching))) {
            cancelPending();
            return false;
        }
        return switching || advanceRequest();
    }

    bool select(const QString& requested)
    {
        return beginRequest() && resolve(requested);
    }

    bool beginDrag(const QString& requested, const QStringList& outputs, POINT press, POINT cursor)
    {
        RECT rect{};
        if (!beginRequest() || !GetWindowRect(m_Window, &rect) || IsRectEmpty(&rect)) return false;
        // Caption tabs are laid out from the right edge. Preserve the grip even
        // when tearing a tab out of a maximized window to its restored width.
        SIZE size{rect.right - rect.left, rect.bottom - rect.top};
        WINDOWPLACEMENT placement{sizeof(placement)};
        if (IsZoomed(m_Window) && GetWindowPlacement(m_Window, &placement) && !IsRectEmpty(&placement.rcNormalPosition)) {
            size = {placement.rcNormalPosition.right - placement.rcNormalPosition.left,
                    placement.rcNormalPosition.bottom - placement.rcNormalPosition.top};
        }
        m_Drag = Drag{cursor, {rect.right - press.x, press.y - rect.top}, size, windowDpi(m_Window), requested};
        m_Drag->pullSelf = requested == m_BoundOutput && m_Window == groupedWindow();
        if (!m_Drag->pullSelf) return resolve(requested);

        // Leave a grouped tab in the original slot. A floating screen keeps its
        // own position, even if it happens to be first in the host's output list.
        QString cold;
        for (const auto& output : outputs) {
            if (output == requested) continue;
            if (HWND peer = find(output)) {
                if (!GetPropW(peer, Detached)) {
                    return resolve(output);
                }
            } else if (cold.isEmpty()) cold = output;
        }
        if (!cold.isEmpty()) return resolve(cold);
        startMoving(m_Window); // All the other tabs already have their own window.
        return true;
    }

    void moveDrag(POINT cursor)
    {
        if (!m_Drag) return;
        m_Drag->cursor = cursor;
        // While the other stream is positioning its HWND, retain only the
        // latest grip position. Button-up always submits the final position.
        if (m_Drag->moveToken && !m_Drag->released) {
            m_Drag->movePending = true;
            return;
        }
        if (member(m_Drag->moving)) positionDrag();
    }

    void dragMoveApplied(WPARAM token)
    {
        if (!m_Drag || !token || m_Drag->moveToken != token) return;
        m_Drag->moveToken = 0;
        if (std::exchange(m_Drag->movePending, false) && !m_Drag->released)
            moveDrag(m_Drag->cursor);
    }

    void acknowledgeDragMove(HWND requester, WPARAM token) const
    {
        if (member(requester) && GetPropW(m_Root, RequestOwner) == requester)
            PostMessageW(requester, DragMoveAppliedMessage, token, 0);
    }

    bool overDockTarget(POINT cursor, HWND preview = nullptr) const
    {
        return m_Drag && ownsRequest() && !ending() && tabBarAt(cursor, m_Drag->moving, preview);
    }

    void finishCaptionMove(POINT cursor)
    {
        if (canDock() && tabBarAt(cursor, m_Window, nullptr))
            dockAsTab(m_BoundOutput, m_Window);
    }

    void finishDrag(POINT cursor, HWND preview = nullptr)
    {
        if (!m_Drag) return;
        if (overDockTarget(cursor, preview)) {
            dockAsTab(m_Drag->output, m_Drag->moving);
            return;
        }
        m_Drag->released = true;
        m_Drag->foregroundAtDrop = GetForegroundWindow();
        moveDrag(cursor);
        if (m_Drag && m_Drag->moving && !m_Drag->awaitingPlacement) activateDrop();
    }

    void cancelPending()
    {
        if (m_Window) RemovePropW(m_Window, WantsLaunch);
        restoreDragZOrder();
        m_Drag.reset();
        m_PendingOutput.clear();
        if (ownsRequest()) {
            RemovePropW(m_Root, RequestOwner);
            RemovePropW(m_Root, SwitchTarget);
            RemovePropW(m_Root, PullingActive);
        }
    }

    bool previewBounds(HWND preview, RECT& rect) const
    {
        if (!m_Drag || !ownsRequest()) return false;
        if (m_Drag->moving && !(m_Drag->awaitingPlacement && IsWindowVisible(preview))) return false;
        rect = dragBounds(IsWindowVisible(preview) ? windowDpi(preview) : m_Drag->sourceDpi);
        return true;
    }

    bool grabPreview(HWND preview, POINT cursor)
    {
        RECT rect{};
        if (!m_Drag || !m_Drag->released || m_Drag->moving || !ownsRequest() || !GetWindowRect(preview, &rect))
            return false;
        const UINT dpi = windowDpi(preview);
        m_Drag->grip = {MulDiv(rect.right - cursor.x, m_Drag->sourceDpi, dpi),
                        MulDiv(cursor.y - rect.top, m_Drag->sourceDpi, dpi)};
        m_Drag->cursor = cursor;
        m_Drag->released = false;
        return true;
    }

    QString pendingTab() const { return !m_Drag && ownsRequest() ? m_PendingOutput : QString{}; }

    void dragWindowPositioned()
    {
        // An idle stream may not otherwise repaint after its HWND becomes visible.
        requestFrame();
        notifyRequester();
    }

    static constexpr UINT SwitchMessage = WM_APP + 0x451;
    static constexpr UINT SwitchedMessage = WM_APP + 0x452;
    static constexpr UINT ReadyMessage = WM_APP + 0x453;
    static constexpr UINT LayoutMessage = WM_APP + 0x454;
    static constexpr UINT DisconnectMessage = WM_APP + 0x455;
    static constexpr UINT DragMoveFenceMessage = WM_APP + 0x456;
    static constexpr UINT DragMoveAppliedMessage = WM_APP + 0x457;

private:
    bool tabBarAt(POINT cursor, HWND moving, HWND preview) const
    {
        const HWND front = groupedWindow();
        if (front == moving || !IsWindowVisible(front) || IsIconic(front)) return false;
        const HWND tabs = static_cast<HWND>(GetPropW(front, TabBar));
        RECT bar{};
        if (!IsWindowVisible(tabs) || GetParent(tabs) != front ||
            !GetWindowRect(tabs, &bar) || !PtInRect(&bar, cursor)) return false;

        // The floating window and its caption normally cover the drop point.
        // Look underneath them for the group's tab bar. A different window
        // covering the bar must still receive an ordinary floating drop.
        HWND hit = GetAncestor(WindowFromPoint(cursor), GA_ROOT);
        for (; hit; hit = GetWindow(hit, GW_HWNDNEXT)) {
            if (hit == tabs || hit == front) return true;
            if (hit == preview || GetAncestor(hit, GA_ROOTOWNER) == moving || !IsWindowVisible(hit)) continue;
            RECT bounds{};
            DWORD cloaked = 0;
            if (GetWindowRect(hit, &bounds) && PtInRect(&bounds, cursor) &&
                (FAILED(DwmGetWindowAttribute(hit, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) || !cloaked))
                return false;
        }
        return false;
    }

    void dockAsTab(QString output, HWND moving)
    {
        if (moving) RemovePropW(moving, Detached);
        // Use the frame-gated switch to select the dropped tab at the original
        // group bounds. The output is copied because select() clears the drag.
        select(output);
    }

    struct Drag {
        POINT cursor, grip;
        SIZE size;
        UINT sourceDpi;
        QString output;
        HWND moving = nullptr, foregroundAtDrop = nullptr;
        bool pullSelf = false, released = false, awaitingPlacement = false, raised = false;
        WPARAM moveToken = 0;
        bool movePending = false;
    };

    bool member(HWND window) const { return window && GetPropW(window, m_GroupProperty.c_str()); }
    bool ending() const { return GetPropW(m_Root, Closing) != nullptr; }
    bool ownsRequest() const { return m_Window && GetPropW(m_Root, RequestOwner) == m_Window; }
    bool beginRequest()
    {
        cancelPending();
        if (!member(m_Root) || ending()) return false;
        RemovePropW(m_Root, SwitchTarget);
        RemovePropW(m_Root, PullingActive);
        SetPropW(m_Root, RequestOwner, m_Window);
        return true;
    }
    bool switchRequested(HWND previous) const
    {
        return member(previous) && !ending() && previous == groupedWindow() && GetPropW(m_Root, SwitchTarget) == m_Window;
    }
    bool cloak(BOOL value) const { return SUCCEEDED(DwmSetWindowAttribute(m_Window, DWMWA_CLOAK, &value, sizeof(value))); }
    void cancelStaging()
    {
        m_StagingBehind = nullptr;
        if (!m_Warming) {
            if (!GetPropW(m_Window, Detached) && m_Window != groupedWindow()) ShowWindow(m_Window, SW_HIDE);
            cloak(false);
        }
    }
    std::wstring property(const QString& output) const
    {
        return (QStringLiteral("Moonlight.Output.") + StreamWindow::outputKey(m_Group, output)).toStdWString();
    }
    void notifyRequester() const
    {
        HWND requester = static_cast<HWND>(GetPropW(m_Root, RequestOwner));
        if (member(requester)) PostMessageW(requester, ReadyMessage, 0, 0);
    }
    void requestFrame()
    {
        m_KeyframeAt = GetTickCount64() + 350;
        m_RequestFrame(false);
    }
    enum class LaunchResult { Started, Busy, Failed };
    LaunchResult launch(const QString& output)
    {
        if (m_Handshake) return LaunchResult::Busy;
        // The gate spans CreateProcess through the host handshake. A click in
        // another stream process can join a launch or go next without racing
        // another /resume against the one Sunshine is already servicing.
        const auto name = L"Local\\" + m_GroupProperty + L".Handshake";
        HANDLE gate = CreateEventW(nullptr, TRUE, FALSE, name.c_str());
        if (!gate) return LaunchResult::Failed;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(gate);
            return LaunchResult::Busy;
        }
        m_Handshake.reset(gate);
        m_AttemptedOutputs.insert(output);
        m_OpeningOutput = output;
        try {
            m_LaunchFuture = std::async(std::launch::async, m_Launch, output, m_Group,
                                        reinterpret_cast<quintptr>(m_Root), m_OutputList);
            return LaunchResult::Started;
        } catch (const std::system_error&) {
            m_Handshake.reset();
            return LaunchResult::Failed;
        }
    }
    void prewarmNext()
    {
        if (m_PrewarmQueue.isEmpty() || !m_PendingOutput.isEmpty() || m_Drag || m_Handshake) return;
        const HWND requester = static_cast<HWND>(GetPropW(m_Root, RequestOwner));
        if (member(requester) && GetPropW(requester, WantsLaunch)) return;
        while (!m_PrewarmQueue.isEmpty()) {
            const QString output = m_PrewarmQueue.takeFirst();
            if (m_AttemptedOutputs.contains(output) || find(output)) continue;
            const auto state = StreamWindow::state(StreamWindow::outputKey(m_Group, output));
            if (state != StreamWindow::None) continue;
            if (launch(output) == LaunchResult::Busy) m_PrewarmQueue.prepend(output);
            break;
        }
    }
    bool collectLaunch()
    {
        if (!m_Handshake) return true;
        if (m_LaunchFuture.valid()) {
            if (m_LaunchFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return true;
            if (!m_LaunchFuture.get() && m_OpeningOutput == m_PendingOutput && m_LaunchedForRequest) {
                m_Handshake.reset();
                cancelPending();
                return false;
            }
        }
        if (StreamWindow::state(StreamWindow::outputKey(m_Group, m_OpeningOutput)) != StreamWindow::Opening) {
            m_Handshake.reset();
            notifyRequester();
        }
        return true;
    }
    HWND find(const QString& output) const
    {
        struct Search { std::wstring key; HWND found; } search{property(output), nullptr};
        EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
            auto& search = *reinterpret_cast<Search*>(data);
            if (!GetPropW(hwnd, search.key.c_str())) return TRUE;
            search.found = hwnd;
            return FALSE;
        }, reinterpret_cast<LPARAM>(&search));
        return search.found;
    }
    bool resolve(const QString& output)
    {
        if (output.isEmpty() || !member(m_Root) || ending()) return false;
        m_PendingOutput = output;
        m_Deadline = GetTickCount64() + 30000;
        m_LaunchedForRequest = false;
        collectLaunch();
        return advanceRequest();
    }
    bool advanceRequest()
    {
        if (HWND target = find(m_PendingOutput)) {
            RemovePropW(m_Window, WantsLaunch);
            if (!m_Drag || m_Drag->pullSelf || GetPropW(target, HasFrame)) present(target);
        } else {
            SetPropW(m_Window, WantsLaunch, reinterpret_cast<HANDLE>(1));
            if ((m_Handshake && m_OpeningOutput == m_PendingOutput) ||
                StreamWindow::state(StreamWindow::outputKey(m_Group, m_PendingOutput)) != StreamWindow::None) return true;
            // If we joined a background prewarm that failed, try the user's
            // request once. A failed explicit launch is reported by the next tick.
            const auto result = m_Launch && !m_LaunchedForRequest ? launch(m_PendingOutput) : LaunchResult::Failed;
            if (result == LaunchResult::Failed) {
                cancelPending();
                return false;
            }
            m_LaunchedForRequest = result == LaunchResult::Started;
        }
        return true;
    }
    void present(HWND target)
    {
        HWND front = groupedWindow();
        if (m_Drag && !m_Drag->pullSelf) {
            startMoving(target);
        } else if (target == front || GetPropW(target, Detached)) {
            m_PendingOutput.clear();
            if (IsIconic(target)) ShowWindowAsync(target, SW_RESTORE);
            if (GetForegroundWindow() != target) SetForegroundWindow(target);
        } else {
            DWORD pid = 0;
            GetWindowThreadProcessId(target, &pid);
            AllowSetForegroundWindow(pid);
            if (GetPropW(target, HasFrame)) m_Deadline = GetTickCount64() + 5000;
            SetPropW(m_Root, SwitchTarget, target);
            if (m_Drag) SetPropW(m_Root, PullingActive, m_Window);
            PostMessageW(target, SwitchMessage, reinterpret_cast<WPARAM>(front), 0);
        }
    }

    void startMoving(HWND moving)
    {
        m_PendingOutput.clear();
        m_Drag->moving = moving;
        // SetWindowPos is asynchronous across stream processes. Retain an
        // existing preview and the drop position until the real window is shown.
        m_Drag->awaitingPlacement = moving != m_Window && !IsWindowVisible(moving);
        if (moving != groupedWindow()) SetPropW(moving, Detached, m_Root);
        if (IsZoomed(moving) || IsIconic(moving)) {
            if (moving == m_Window) ShowWindow(moving, SW_SHOWNOACTIVATE);
            else ShowWindowAsync(moving, SW_SHOWNOACTIVATE);
        }
        if (!m_Drag) return; // Restoring a window may change mouse capture.
        positionDrag(true);
        PostMessageW(moving, LayoutMessage, 0, 0);
        if (m_Drag && m_Drag->released && !m_Drag->awaitingPlacement) activateDrop();
    }
    RECT dragBounds(UINT scale) const
    {
        const auto& drag = *m_Drag;
        const LONG w = std::max(1, MulDiv(drag.size.cx, scale, drag.sourceDpi));
        const LONG h = std::max(1, MulDiv(drag.size.cy, scale, drag.sourceDpi));
        const LONG grip = std::clamp<LONG>(MulDiv(drag.grip.x, scale, drag.sourceDpi), 1, w);
        const LONG x = drag.cursor.x - w + grip;
        LONG y = drag.cursor.y - MulDiv(drag.grip.y, scale, drag.sourceDpi);
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromPoint(drag.cursor, MONITOR_DEFAULTTONEAREST), &monitor)) {
            const LONG caption = std::min<LONG>(MulDiv(32, scale, 96), h);
            y = std::clamp(y, monitor.rcWork.top, monitor.rcWork.bottom - caption);
        }
        return {x, y, x + w, y + h};
    }
    void positionDrag(bool first = false)
    {
        HWND moving = m_Drag->moving;
        RECT current{};
        GetWindowRect(moving, &current);
        const RECT desired = dragBounds(windowDpi(moving));
        if (!first && !m_Drag->moveToken && EqualRect(&current, &desired)) return;
        UINT flags = SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS | (first ? SWP_SHOWWINDOW : SWP_NOZORDER);
        if (desired.right - desired.left == current.right - current.left &&
            desired.bottom - desired.top == current.bottom - current.top) flags |= SWP_NOSIZE;
        HWND after = HWND_TOP;
        if (first && !m_Drag->released && !(GetWindowLongPtrW(moving, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
            // Mouse capture stays on the originating tab. Windows may put a
            // nonactivating show from another process behind its foreground window.
            after = HWND_TOPMOST;
            m_Drag->raised = true;
        } else if (first && m_Drag->released && !dropCanTakeFocus() && moving != m_Window) {
            // The connection finished after the user switched to another app.
            after = m_Window;
        }
        if (SetWindowPos(moving, after, desired.left, desired.top, desired.right - desired.left,
                         desired.bottom - desired.top, flags) &&
            moving != m_Window && m_Drag && !m_Drag->released) {
            // The peer processes the asynchronous SetWindowPos before this
            // message. Ignore acknowledgements from a previous drag.
            if (!++m_NextMoveToken) ++m_NextMoveToken;
            if (PostMessageW(moving, DragMoveFenceMessage, m_NextMoveToken, reinterpret_cast<LPARAM>(m_Window)))
                m_Drag->moveToken = m_NextMoveToken;
        }
    }
    void restoreDragZOrder()
    {
        if (m_Drag && std::exchange(m_Drag->raised, false) && member(m_Drag->moving)) {
            SetWindowPos(m_Drag->moving, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        }
    }
    bool dropCanTakeFocus() const
    {
        // A quick drag on a background tab can finish before its owner gets
        // activated. Honor the drop unless focus moved elsewhere afterwards.
        const HWND foreground = GetForegroundWindow();
        return member(foreground) || (foreground && foreground == m_Drag->foregroundAtDrop);
    }
    void activateDrop()
    {
        if (member(m_Drag->moving) && dropCanTakeFocus() && GetForegroundWindow() != m_Drag->moving)
            SetForegroundWindow(m_Drag->moving);
        restoreDragZOrder();
        m_Drag.reset();
    }

    QStringList m_PrewarmQueue;
    QString m_OpeningOutput;
    QString m_OutputList;
    QSet<QString> m_AttemptedOutputs;
    Launch m_Launch;
    std::function<void(bool)> m_RequestFrame;
    std::unique_ptr<void, decltype(&CloseHandle)> m_Handshake{nullptr, &CloseHandle};
    std::future<bool> m_LaunchFuture;
    ULONGLONG m_Deadline = 0, m_KeyframeAt = 0;
    bool m_Warming = false, m_LaunchedForRequest = false;
    const QString m_Output, m_Group;
    const std::wstring m_GroupProperty;
    QString m_BoundOutput, m_PendingOutput;
    std::wstring m_OutputProperty;
    HWND m_Root = nullptr, m_Window = nullptr, m_StagingBehind = nullptr;
    std::optional<Drag> m_Drag;
    WPARAM m_NextMoveToken = 0;
    static constexpr auto Detached = L"Moonlight.Display.Detached";
    static constexpr auto TabBar = L"Moonlight.Display.TabBar";
    static constexpr auto HasFrame = L"Moonlight.Display.HasFrame";
    static constexpr auto RequestOwner = L"Moonlight.Display.RequestOwner";
    static constexpr auto WantsLaunch = L"Moonlight.Display.WantsLaunch";
    static constexpr auto SwitchTarget = L"Moonlight.Display.SwitchTarget";
    static constexpr auto PullingActive = L"Moonlight.Display.PullingActive";
    static constexpr auto Closing = L"Moonlight.Display.Closing";
};
