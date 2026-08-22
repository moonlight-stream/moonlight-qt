#include "stylusreplaytest.h"

#include "stylusreplay.h"
#include "streaming/video/stylusreplaypanel.h"

#include <Limelight.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QtGlobal>

#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace {

std::uint64_t monotonicUs()
{
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const Uint64 counter = SDL_GetPerformanceCounter();
    if (frequency == 0) {
        return static_cast<std::uint64_t>(SDL_GetTicks()) * 1000ULL;
    }
    return static_cast<std::uint64_t>(counter / frequency) * 1000000ULL +
            static_cast<std::uint64_t>((counter % frequency) * 1000000ULL / frequency);
}

HWND sdlWindowHandle(SDL_Window* window)
{
    if (!window) {
        return nullptr;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        return info.info.win.window;
    }
    return nullptr;
}

struct FileSelection {
    QString path;
    DWORD errorCode = 0;
};

FileSelection selectRecordingFile(HWND owner,
                                  const QString& initialDirectory,
                                  const QString& recordingFilterLabel,
                                  const QString& allFilesFilterLabel,
                                  const QString& title)
{
    std::array<wchar_t, 32768> filePath{};
    // OPENFILENAMEW expects pairs of labels and patterns separated by NUL,
    // followed by a second NUL at the end of the complete filter list.
    std::wstring filter = recordingFilterLabel.toStdWString();
    filter.push_back(L'\0');
    filter.append(L"*.dat");
    filter.push_back(L'\0');
    filter.append(allFilesFilterLabel.toStdWString());
    filter.push_back(L'\0');
    filter.append(L"*.*");
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    const std::wstring nativeTitle = title.toStdWString();
    const std::wstring nativeInitialDirectory =
            QDir::toNativeSeparators(initialDirectory).toStdWString();
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter.c_str();
    dialog.lpstrFile = filePath.data();
    dialog.nMaxFile = static_cast<DWORD>(filePath.size());
    dialog.lpstrInitialDir = nativeInitialDirectory.empty() ?
            nullptr : nativeInitialDirectory.c_str();
    dialog.lpstrTitle = nativeTitle.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = L"dat";

    if (!GetOpenFileNameW(&dialog)) {
        return {QString(), CommDlgExtendedError()};
    }
    return {QString::fromWCharArray(filePath.data()), 0};
}

bool deadlineReached(Uint32 now, Uint32 deadline)
{
    return static_cast<Sint32>(now - deadline) >= 0;
}

}

class StylusReplayTest::Impl
{
public:
    Impl(SDL_Window* streamingWindow,
         ToastCallback toastCallback,
         PanelClosedCallback panelClosedCallback)
        : m_StreamingWindow(streamingWindow),
          m_ToastCallback(std::move(toastCallback)),
          m_PanelClosedCallback(std::move(panelClosedCallback)),
          m_Panel(std::make_unique<StylusReplayPanel>()),
          m_Speed(1),
          m_FilterLocalMouseInput(true),
          m_PendingShow(false),
          m_PendingImport(false),
          m_ShowDeadlineTicks(0),
          m_LastUiUpdateTicks(0)
    {
        if (HWND streamWindow = sdlWindowHandle(m_StreamingWindow)) {
            SetWindowLongPtrW(reinterpret_cast<HWND>(m_Panel->winId()),
                              GWLP_HWNDPARENT,
                              reinterpret_cast<LONG_PTR>(streamWindow));
        }

        m_Panel->setActionCallback([this](StylusReplayPanel::Action action) {
            handlePanelAction(action);
        });
        m_Panel->setCloseCallback([this]() {
            m_PendingImport = false;
            stop(true);
            if (m_PanelClosedCallback) {
                m_PanelClosedCallback();
            }
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stylus replay test panel closed");
        });
        updatePanelState();
    }

    ~Impl()
    {
        m_PendingShow = false;
        m_PendingImport = false;
        m_Panel->setActionCallback({});
        m_Panel->setCloseCallback({});
        stop(false);
        m_Panel->closePanel();
    }

    void requestShow(const QRect& parentGeometry)
    {
        if (m_Panel->isVisible()) {
            m_Panel->showPanel(parentGeometry);
            return;
        }

        // Let the overlay menu finish its short close animation before the
        // focusable test window appears. The delay is handled by process(),
        // never by a blocking sleep or nested event loop.
        m_PendingGeometry = parentGeometry;
        m_PendingShow = true;
        m_ShowDeadlineTicks = SDL_GetTicks() + 170;
    }

    void closePanel()
    {
        const bool canceledPendingShow = m_PendingShow;
        m_PendingShow = false;
        if (m_Panel->isVisible()) {
            m_Panel->closePanel();
        }
        else if (canceledPendingShow && m_PanelClosedCallback) {
            // requestShow() already took ownership of the saved capture state.
            // Return it even if the stream is hidden before the delayed panel
            // show occurs.
            m_PanelClosedCallback();
        }
    }

    void process()
    {
        const Uint32 nowTicks = SDL_GetTicks();
        if (m_PendingShow && deadlineReached(nowTicks, m_ShowDeadlineTicks)) {
            m_PendingShow = false;
            updatePanelState();
            m_Panel->showPanel(m_PendingGeometry);
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stylus replay test panel shown");
        }

        if (m_PendingImport) {
            m_PendingImport = false;
            importRecording();
        }

        processReplay();
    }

    int nextDelayMs() const
    {
        int delay = m_Replay.nextDelayMs(monotonicUs());
        if (m_PendingShow) {
            const Uint32 now = SDL_GetTicks();
            const int showDelay = deadlineReached(now, m_ShowDeadlineTicks) ? 0 :
                    static_cast<int>(m_ShowDeadlineTicks - now);
            delay = delay < 0 ? showDelay : std::min(delay, showDelay);
        }
        return delay;
    }

    void stop(bool notifyUser)
    {
        const bool wasPlaying = m_Replay.isPlaying();
        m_Replay.stop();
        m_LastUiUpdateTicks = 0;
        if (wasPlaying) {
            sendCancel();
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stylus replay stopped");
        }
        updatePanelState();
        if (notifyUser && wasPlaying) {
            toast(tr("Stylus replay stopped."));
        }
    }

    bool shouldFilterLocalMouseInput() const
    {
        return m_FilterLocalMouseInput && m_Replay.isPlaying();
    }

    bool isPanelVisible() const
    {
        return m_Panel->isVisible();
    }

private:
    static QString tr(const char* text)
    {
        return QCoreApplication::translate("StylusReplayTest", text);
    }

    void toast(const QString& message, int durationMs = 2000)
    {
        if (m_ToastCallback) {
            m_ToastCallback(message, durationMs);
        }
    }

    bool hostSupportsStylus() const
    {
        return (LiGetHostFeatureFlags() & LI_FF_PEN_TOUCH_EVENTS) != 0;
    }

    void handlePanelAction(StylusReplayPanel::Action action)
    {
        switch (action) {
        case StylusReplayPanel::Action::ChooseRecording:
            if (m_Replay.isPlaying()) {
                toast(tr("Stop stylus replay before importing another recording."));
            }
            else if (!hostSupportsStylus()) {
                toast(tr("The connected host does not support stylus input."), 3500);
            }
            else {
                // Defer the native dialog until after the current Qt mouse
                // callback returns.
                m_PendingImport = true;
            }
            break;
        case StylusReplayPanel::Action::StartReplay:
            startReplay();
            break;
        case StylusReplayPanel::Action::StopReplay:
            stop(true);
            break;
        case StylusReplayPanel::Action::SetSpeed1:
            setSpeed(1);
            break;
        case StylusReplayPanel::Action::SetSpeed2:
            setSpeed(2);
            break;
        case StylusReplayPanel::Action::SetSpeed4:
            setSpeed(4);
            break;
        case StylusReplayPanel::Action::ToggleMouseFilter:
            if (!m_Replay.isPlaying()) {
                m_FilterLocalMouseInput = !m_FilterLocalMouseInput;
                updatePanelState();
            }
            break;
        }
    }

    void setSpeed(int speed)
    {
        if (m_Replay.isPlaying()) {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                         "Stylus replay speed change ignored while playing");
            return;
        }
        m_Speed = speed;
        updatePanelState();
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Stylus replay speed selected: %dx", m_Speed);
    }

    void importRecording()
    {
        HWND owner = m_Panel->isVisible() ?
                reinterpret_cast<HWND>(m_Panel->winId()) :
                sdlWindowHandle(m_StreamingWindow);
        const FileSelection selection =
                selectRecordingFile(owner,
                                    m_LastDirectory,
                                    tr("Stylus recording files (*.dat)"),
                                    tr("All files (*.*)"),
                                    tr("Import stylus recording file"));
        if (!selection.path.isEmpty()) {
            m_LastDirectory = QFileInfo(selection.path).absolutePath();
            QString error;
            if (m_Replay.loadFile(selection.path, error)) {
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                            "Stylus replay recording imported: samples=%llu durationUs=%llu truncated=%d",
                            static_cast<unsigned long long>(m_Replay.sampleCount()),
                            static_cast<unsigned long long>(m_Replay.durationUs()),
                            m_Replay.isTruncated() ? 1 : 0);
                updatePanelState();
                QString message = tr("Stylus recording loaded: %1 samples, %2 seconds")
                        .arg(static_cast<qulonglong>(m_Replay.sampleCount()))
                        .arg(m_Replay.durationUs() / 1000000.0, 0, 'f', 2);
                if (m_Replay.isTruncated()) {
                    message += tr(" (source recording is truncated)");
                }
                toast(message, 3500);
            }
            else {
                toast(tr("Unable to import stylus recording: %1").arg(error), 4500);
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Stylus replay recording rejected");
            }
        }
        else if (selection.errorCode != 0) {
            toast(tr("Unable to open the recording picker (error %1)")
                          .arg(selection.errorCode), 4000);
        }
        else {
            toast(tr("No recording was selected."));
        }

        if (m_Panel->isVisible()) {
            m_Panel->raise();
            m_Panel->requestActivate();
        }
    }

    void startReplay()
    {
        if (!hostSupportsStylus()) {
            toast(tr("The connected host does not support stylus input."), 3500);
            return;
        }
        if (m_Replay.isPlaying()) {
            toast(tr("Stylus replay is already running."));
            return;
        }

        QString error;
        if (!m_Replay.start(monotonicUs(), m_Speed, error)) {
            toast(error, 3500);
            return;
        }
        if (!sendCancel()) {
            m_Replay.stop();
            updatePanelState();
            toast(tr("The stylus input queue is not available."), 3500);
            return;
        }

        updatePanelState();
        QString message = tr("Replaying %1 at %2x: %3 samples, %4 seconds")
                .arg(m_Replay.sourceFileName())
                .arg(m_Speed)
                .arg(static_cast<qulonglong>(m_Replay.sampleCount()))
                .arg(m_Replay.durationUs() / 1000000.0, 0, 'f', 2);
        if (m_Replay.isTruncated()) {
            message += tr(" (source recording is truncated)");
        }
        toast(message, 4500);
        m_LastUiUpdateTicks = 0;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Stylus replay started: samples=%llu speed=%dx mouseFilter=%d",
                    static_cast<unsigned long long>(m_Replay.sampleCount()),
                    m_Speed,
                    m_FilterLocalMouseInput ? 1 : 0);
    }

    void processReplay()
    {
        if (!m_Replay.isPlaying()) {
            return;
        }

        const auto result = m_Replay.processDue(
                monotonicUs(), [](const StylusReplayController::Sample& sample) {
            const bool isCancel = sample.eventType == LI_TOUCH_EVENT_CANCEL ||
                    sample.eventType == LI_TOUCH_EVENT_CANCEL_ALL;
            return LiSendPenEvent(sample.eventType,
                                  isCancel ? LI_TOOL_TYPE_UNKNOWN : LI_TOOL_TYPE_PEN,
                                  0,
                                  sample.x, sample.y, sample.pressure,
                                  0.0f, 0.0f,
                                  sample.rotation, sample.tilt);
        });

        if (result == StylusReplayController::ProcessResult::Finished) {
            sendCancel();
            updatePanelState();
            toast(tr("Stylus replay completed."));
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Stylus replay completed");
        }
        else if (result == StylusReplayController::ProcessResult::SendFailed) {
            sendCancel();
            updatePanelState();
            toast(tr("Stylus replay stopped because the input queue rejected an event."),
                  4000);
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Stylus replay stopped after an input queue error");
        }
        else if (result == StylusReplayController::ProcessResult::Running &&
                 m_Panel->isVisible()) {
            const Uint32 now = SDL_GetTicks();
            if (now - m_LastUiUpdateTicks >= 250) {
                m_LastUiUpdateTicks = now;
                updatePanelState();
            }
        }
    }

    void updatePanelState()
    {
        QString summary = tr("No recording");
        QString status;
        int progress = -1;
        if (m_Replay.isLoaded()) {
            summary = tr("%1 samples · %2 s")
                    .arg(static_cast<qulonglong>(m_Replay.sampleCount()))
                    .arg(m_Replay.durationUs() / 1000000.0, 0, 'f', 1);
        }
        if (m_Replay.isPlaying()) {
            const std::size_t totalSamples = m_Replay.sampleCount();
            progress = totalSamples == 0 ? 0 : qBound(
                    0,
                    static_cast<int>(m_Replay.processedSampleCount() * 100 / totalSamples),
                    100);
            const double remainingSeconds = m_Replay.remainingUs(monotonicUs()) / 1000000.0;
            status = tr("%1% · %2 s left").arg(progress).arg(remainingSeconds, 0, 'f', 1);
        }
        m_Panel->updateState(summary, status, m_Speed, progress,
                             m_Replay.isLoaded(), m_Replay.isPlaying(),
                             hostSupportsStylus(), m_FilterLocalMouseInput);
    }

    bool sendCancel()
    {
        return LiSendPenEvent(LI_TOUCH_EVENT_CANCEL_ALL, LI_TOOL_TYPE_UNKNOWN, 0,
                              0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              LI_ROT_UNKNOWN, LI_TILT_UNKNOWN) == 0;
    }

    SDL_Window* m_StreamingWindow;
    ToastCallback m_ToastCallback;
    PanelClosedCallback m_PanelClosedCallback;
    std::unique_ptr<StylusReplayPanel> m_Panel;
    StylusReplayController m_Replay;
    QString m_LastDirectory;
    QRect m_PendingGeometry;
    int m_Speed;
    bool m_FilterLocalMouseInput;
    bool m_PendingShow;
    bool m_PendingImport;
    Uint32 m_ShowDeadlineTicks;
    Uint32 m_LastUiUpdateTicks;
};

StylusReplayTest::StylusReplayTest(SDL_Window* streamingWindow,
                                   ToastCallback toastCallback,
                                   PanelClosedCallback panelClosedCallback)
    : m_Impl(std::make_unique<Impl>(streamingWindow,
                                   std::move(toastCallback),
                                   std::move(panelClosedCallback)))
{
}

StylusReplayTest::~StylusReplayTest() = default;

void StylusReplayTest::requestShow(const QRect& parentGeometry)
{
    m_Impl->requestShow(parentGeometry);
}

void StylusReplayTest::closePanel()
{
    m_Impl->closePanel();
}

bool StylusReplayTest::isPanelVisible() const
{
    return m_Impl->isPanelVisible();
}

void StylusReplayTest::process()
{
    m_Impl->process();
}

int StylusReplayTest::nextDelayMs() const
{
    return m_Impl->nextDelayMs();
}

void StylusReplayTest::stop(bool notifyUser)
{
    m_Impl->stop(notifyUser);
}

bool StylusReplayTest::shouldFilterLocalMouseInput() const
{
    return m_Impl->shouldFilterLocalMouseInput();
}
