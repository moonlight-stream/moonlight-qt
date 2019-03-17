#include "dxvsyncsource.h"

// Useful references:
// https://bugs.chromium.org/p/chromium/issues/detail?id=467617
// https://chromium.googlesource.com/chromium/src.git/+/c564f2fe339b2b2abb0c8773c90c83215670ea71/gpu/ipc/service/gpu_vsync_provider_win.cc

DxVsyncSource::DxVsyncSource(Pacer* pacer) :
    m_Pacer(pacer),
    m_Thread(nullptr),
    m_Gdi32Handle(nullptr)
{
    SDL_AtomicSet(&m_Stopping, 0);
}

DxVsyncSource::~DxVsyncSource()
{
    if (m_Thread != nullptr) {
        SDL_AtomicSet(&m_Stopping, 1);
        SDL_WaitThread(m_Thread, nullptr);
    }

    if (m_Gdi32Handle != nullptr) {
        FreeLibrary(m_Gdi32Handle);
    }
}

bool DxVsyncSource::initialize(SDL_Window* window, int displayFps)
{
    m_DisplayFps = displayFps;

    m_Gdi32Handle = LoadLibraryA("gdi32.dll");
    if (m_Gdi32Handle == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to load gdi32.dll: %d",
                     GetLastError());
        return false;
    }

    m_D3DKMTOpenAdapterFromHdc = (PFND3DKMTOPENADAPTERFROMHDC)GetProcAddress(m_Gdi32Handle, "D3DKMTOpenAdapterFromHdc");
    m_D3DKMTCloseAdapter = (PFND3DKMTCLOSEADAPTER)GetProcAddress(m_Gdi32Handle, "D3DKMTCloseAdapter");
    m_D3DKMTWaitForVerticalBlankEvent = (PFND3DKMTWAITFORVERTICALBLANKEVENT)GetProcAddress(m_Gdi32Handle, "D3DKMTWaitForVerticalBlankEvent");

    if (m_D3DKMTOpenAdapterFromHdc == nullptr ||
            m_D3DKMTCloseAdapter == nullptr ||
            m_D3DKMTWaitForVerticalBlankEvent == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Missing required function in gdi32.dll");
        return false;
    }

    SDL_SysWMinfo info;

    SDL_VERSION(&info.version);

    if (!SDL_GetWindowWMInfo(window, &info)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "SDL_GetWindowWMInfo() failed: %s",
                     SDL_GetError());
        return false;
    }

    m_Window = info.info.win.window;

    m_Thread = SDL_CreateThread(vsyncThread, "DX Vsync Thread", this);
    if (m_Thread == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to create DX V-sync thread: %s",
                     SDL_GetError());
        return false;
    }

    return true;
}

int DxVsyncSource::vsyncThread(void* context)
{
    DxVsyncSource* me = reinterpret_cast<DxVsyncSource*>(context);

#if SDL_VERSION_ATLEAST(2, 0, 9)
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
#else
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
#endif

    D3DKMT_OPENADAPTERFROMHDC openAdapterParams = {};
    HMONITOR lastMonitor = nullptr;
    DEVMODEA monitorMode;
    monitorMode.dmSize = sizeof(monitorMode);

    while (SDL_AtomicGet(&me->m_Stopping) == 0) {
        D3DKMT_WAITFORVERTICALBLANKEVENT waitForVblankEventParams;
        NTSTATUS status;

        // If the monitor has changed from last time, open the new adapter
        HMONITOR currentMonitor = MonitorFromWindow(me->m_Window, MONITOR_DEFAULTTONEAREST);
        if (currentMonitor != lastMonitor) {
            MONITORINFOEXA monitorInfo = {};
            monitorInfo.cbSize = sizeof(monitorInfo);
            if (!GetMonitorInfoA(currentMonitor, &monitorInfo)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "GetMonitorInfo() failed: %d",
                             GetLastError());
                SDL_Delay(10);
                continue;
            }

            if (!EnumDisplaySettingsA(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &monitorMode)) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "EnumDisplaySettings() failed: %d",
                             GetLastError());
                SDL_Delay(10);
                continue;
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Monitor changed: %s %d Hz",
                        monitorInfo.szDevice,
                        monitorMode.dmDisplayFrequency);

            if (openAdapterParams.hAdapter != 0) {
                D3DKMT_CLOSEADAPTER closeAdapterParams = {};
                closeAdapterParams.hAdapter = openAdapterParams.hAdapter;
                me->m_D3DKMTCloseAdapter(&closeAdapterParams);
            }

            openAdapterParams.hDc = CreateDCA(nullptr, monitorInfo.szDevice, nullptr, nullptr);
            if (!openAdapterParams.hDc) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "CreateDC() failed: %d",
                             GetLastError());
                SDL_Delay(10);
                continue;
            }

            status = me->m_D3DKMTOpenAdapterFromHdc(&openAdapterParams);
            DeleteDC(openAdapterParams.hDc);

            if (status != STATUS_SUCCESS) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                             "D3DKMTOpenAdapterFromHdc() failed: %x",
                             status);
                SDL_Delay(10);
                continue;
            }

            lastMonitor = currentMonitor;
        }

        waitForVblankEventParams.hAdapter = openAdapterParams.hAdapter;
        waitForVblankEventParams.hDevice = 0;
        waitForVblankEventParams.VidPnSourceId = openAdapterParams.VidPnSourceId;

        status = me->m_D3DKMTWaitForVerticalBlankEvent(&waitForVblankEventParams);
        if (status != STATUS_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "D3DKMTWaitForVerticalBlankEvent() failed: %x",
                         status);
            SDL_Delay(10);
            continue;
        }

        me->m_Pacer->vsyncCallback(1000 / me->m_DisplayFps);
    }

    if (openAdapterParams.hAdapter != 0) {
        D3DKMT_CLOSEADAPTER closeAdapterParams = {};
        closeAdapterParams.hAdapter = openAdapterParams.hAdapter;
        me->m_D3DKMTCloseAdapter(&closeAdapterParams);
    }

    return 0;
}
