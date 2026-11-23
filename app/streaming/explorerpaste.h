#pragma once

#include "clipboard_win32.h"
#include <exdisp.h>
#include <servprov.h>
#include <shlguid.h>
#include <wrl/client.h>
#include <QTimer>
#include <functional>

// Resolve the focused folder view, including the active tab on Windows 11.
namespace explorer_paste {
    using Microsoft::WRL::ComPtr;
    using directory_t = std::filesystem::path;

    inline std::wstring window_class(HWND window) {
      wchar_t name[128] {};
      GetClassNameW(window, name, 128);
      return name;
    }

    inline HWND focused_pane(HWND window, bool desktop_window) {
      GUITHREADINFO info {sizeof(info)};
      if (!GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &info) || !info.hwndFocus || GetAncestor(info.hwndFocus, GA_ROOT) != window || info.hwndMenuOwner || info.flags & (GUI_INMENUMODE | GUI_POPUPMENUMODE | GUI_SYSTEMMENUMODE)) {
        return nullptr;
      }
      for (auto child = info.hwndFocus; child && child != window; child = GetParent(child)) {
        const auto name = window_class(child);
        if (name == L"Edit" || name.find(L"SearchBox") != std::wstring::npos || name.find(L"Address Band") != std::wstring::npos) {
          return nullptr;
        }
        if (name == L"SHELLDLL_DefView" || name == L"SysListView32") {
          return child;
        }
      }
      return desktop_window && info.hwndFocus == window ? window : nullptr;
    }

    inline directory_t explorer_directory(HWND window, HWND pane) {
      ComPtr<IShellWindows> windows;
      if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING, IID_PPV_ARGS(windows.GetAddressOf())))) {
        return {};
      }
      long count {};
      if (FAILED(windows->get_Count(&count))) {
        return {};
      }
      for (long i = 0; i < count; ++i) {
        VARIANT index {};
        index.vt = VT_I4;
        index.lVal = i;
        ComPtr<IDispatch> dispatch;
        ComPtr<IWebBrowserApp> browser;
        SHANDLE_PTR hwnd {};
        if (FAILED(windows->Item(index, &dispatch)) || !dispatch || FAILED(dispatch.As(&browser)) || FAILED(browser->get_HWND(&hwnd)) || reinterpret_cast<HWND>(hwnd) != window) {
          continue;
        }
        ComPtr<IServiceProvider> services;
        ComPtr<IShellBrowser> shell;
        ComPtr<IShellView> view;
        HWND view_window {};
        if (FAILED(dispatch.As(&services)) || FAILED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(shell.GetAddressOf()))) || FAILED(shell->QueryActiveShellView(&view)) || FAILED(view->GetWindow(&view_window)) || (view_window != pane && !IsChild(view_window, pane))) {
          continue;
        }
        ComPtr<IFolderView> folder_view;
        ComPtr<IPersistFolder2> folder;
        PIDLIST_ABSOLUTE id {};
        if (FAILED(view.As(&folder_view)) || FAILED(folder_view->GetFolder(IID_PPV_ARGS(folder.GetAddressOf()))) || FAILED(folder->GetCurFolder(&id)) || !id) {
          continue;
        }
        std::unique_ptr<ITEMIDLIST, decltype(&CoTaskMemFree)> release_id(id, &CoTaskMemFree);
        ComPtr<IShellItem> item;
        PWSTR path {};
        if (SUCCEEDED(SHCreateItemFromIDList(id, IID_PPV_ARGS(item.GetAddressOf()))) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
          std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> release(path, &CoTaskMemFree);
          return directory_t(path);
        }
      }
      return {};
    }

    struct Target {
        HWND window = nullptr;
        HWND pane = nullptr;
        directory_t directory;
    };

    inline Target focusedTarget() {
        Target target;
        target.window = GetForegroundWindow();
        const auto name = window_class(target.window);
        const bool desktop = name == L"Progman" || name == L"WorkerW";
        if (!desktop && name != L"CabinetWClass" && name != L"ExploreWClass") return {};
        target.pane = focused_pane(target.window, desktop);
        if (!target.pane) return {};
        if (desktop) {
            DWORD owner{}, shell{};
            GetWindowThreadProcessId(target.window, &owner);
            GetWindowThreadProcessId(GetShellWindow(), &shell);
            PWSTR folder = nullptr;
            if (!shell || shell != owner || FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &folder))) return {};
            target.directory = folder;
            CoTaskMemFree(folder);
        } else {
            target.directory = explorer_directory(target.window, target.pane);
        }
        return target;
    }

    // Only consume the paste chord in a verified Explorer folder view. The
    // keyboard callback never calls Explorer COM or waits on disk/network I/O.
    class Shortcut : public QObject {
    public:
        using Paste = std::function<void(const Target&)>;
        ~Shortcut() { reset(); }
        void reset() {
            if (hook) UnhookWindowsHookEx(hook);
            hook = nullptr;
            if (active == this) active = nullptr;
            action = {};
            target = {};
            down = false;
        }
        void update(DWORD sequence, Paste paste) {
            if (busy) return;
            target = focusedTarget();
            if (target.directory.empty() || !win_clipboard::detail::local_file_path(target.directory)) {
                reset();
                return;
            }
            revision = sequence;
            action = std::move(paste);
            if (!hook) {
                active = this;
                hook = SetWindowsHookExW(WH_KEYBOARD_LL, callback, GetModuleHandleW(nullptr), 0);
            }
        }
    private:
        static LRESULT CALLBACK callback(int code, WPARAM event, LPARAM data) {
            auto self = active;
            if (code == HC_ACTION && self && reinterpret_cast<KBDLLHOOKSTRUCT*>(data)->vkCode == 'V') {
                if (event == WM_KEYUP && self->down) { self->down = false; return 1; }
                if (event == WM_KEYDOWN && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                    !(GetAsyncKeyState(VK_MENU) & 0x8000) && !(GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                    GetForegroundWindow() == self->target.window && self->revision == GetClipboardSequenceNumber() &&
                    focused_pane(self->target.window, self->target.pane == self->target.window) == self->target.pane) {
                    if (!self->down && !self->busy) {
                        self->busy = true;
                        auto captured = self->target;
                        auto action = self->action;
                        QTimer::singleShot(0, self, [self, captured, action] {
                            action(captured);
                            self->busy = false;
                        });
                    }
                    self->down = true;
                    return 1;
                }
            }
            return CallNextHookEx(nullptr, code, event, data);
        }
        inline static thread_local Shortcut* active = nullptr;
        Target target;
        Paste action;
        HHOOK hook = nullptr;
        DWORD revision = 0;
        bool busy = false;
        bool down = false;
    };
}
