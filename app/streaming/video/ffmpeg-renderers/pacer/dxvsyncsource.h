#pragma once

#include "pacer.h"

#if HAVE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL_syswm.h>
#endif

// from <D3dkmthk.h>
typedef LONG NTSTATUS;
typedef UINT D3DKMT_HANDLE;
typedef UINT D3DDDI_VIDEO_PRESENT_SOURCE_ID;
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_GRAPHICS_PRESENT_OCCLUDED ((NTSTATUS)0xC01E0006L)
typedef struct _D3DKMT_OPENADAPTERFROMHDC {
  HDC hDc;
  D3DKMT_HANDLE hAdapter;
  LUID AdapterLuid;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_OPENADAPTERFROMHDC;
typedef struct _D3DKMT_CLOSEADAPTER {
  D3DKMT_HANDLE hAdapter;
} D3DKMT_CLOSEADAPTER;
typedef struct _D3DKMT_WAITFORVERTICALBLANKEVENT {
  D3DKMT_HANDLE hAdapter;
  D3DKMT_HANDLE hDevice;
  D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId;
} D3DKMT_WAITFORVERTICALBLANKEVENT;
typedef NTSTATUS(APIENTRY* PFND3DKMTOPENADAPTERFROMHDC)(D3DKMT_OPENADAPTERFROMHDC*);
typedef NTSTATUS(APIENTRY* PFND3DKMTCLOSEADAPTER)(D3DKMT_CLOSEADAPTER*);
typedef NTSTATUS(APIENTRY* PFND3DKMTWAITFORVERTICALBLANKEVENT)(D3DKMT_WAITFORVERTICALBLANKEVENT*);

class DxVsyncSource : public IVsyncSource
{
public:
    DxVsyncSource(Pacer* pacer);

    virtual ~DxVsyncSource();

    virtual bool initialize(SDL_Window* window, int) override;

    virtual bool isAsync() override;

    virtual void waitForVsync() override;

private:
    Pacer* m_Pacer;
    HMODULE m_Gdi32Handle;
    HWND m_Window;
    HMONITOR m_LastMonitor;
    D3DKMT_WAITFORVERTICALBLANKEVENT m_WaitForVblankEventParams;

    PFND3DKMTOPENADAPTERFROMHDC m_D3DKMTOpenAdapterFromHdc;
    PFND3DKMTCLOSEADAPTER m_D3DKMTCloseAdapter;
    PFND3DKMTWAITFORVERTICALBLANKEVENT m_D3DKMTWaitForVerticalBlankEvent;
};
