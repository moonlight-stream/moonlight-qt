/**
 * @file shellfiledrop.h
 * @brief Resolve a file drag in a Windows Explorer folder view, including the desktop.
 * Shared with the Windows Sunshine host. Coordinates are physical screen pixels.
 */
#pragma once

#include <exdisp.h>
#include <memory>
#include <shlguid.h>
#include <shlobj.h>
#include <string>
#include <uiautomationclient.h>
#include <vector>
#include <windows.h>
#include <wrl/client.h>

namespace shell_file_drop {
  using Microsoft::WRL::ComPtr;

  /** @brief Return the native class of the window under a drag. */
  inline std::wstring window_class(HWND window) {
    wchar_t name[128] {};
    GetClassNameW(window, name, 128);
    return name;
  }

  /** @brief Check the top-level window before calling Explorer or UI Automation. */
  inline bool shell_window(HWND window) {
    const auto name = window_class(GetAncestor(window, GA_ROOT));
    return name == L"CabinetWClass" || name == L"ExploreWClass" || name == L"WorkerW" || name == L"Progman";
  }

  /** @brief Match the actual shell view below the pointer, including the visible tab. */
  inline ComPtr<IFolderView2> folder_view(IDispatch *dispatch, HWND hit) {
    ComPtr<IServiceProvider> services;
    ComPtr<IShellBrowser> browser;
    ComPtr<IShellView> view;
    ComPtr<IFolderView2> folder;
    HWND window {};
    if (dispatch && SUCCEEDED(dispatch->QueryInterface(IID_PPV_ARGS(&services))) && SUCCEEDED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser))) && SUCCEEDED(browser->QueryActiveShellView(&view)) && SUCCEEDED(view->GetWindow(&window)) && (window == hit || IsChild(window, hit))) {
      view.As(&folder);
    }
    return folder;
  }

  /** @brief Obtain the view for a point without activating a window or changing selection. */
  inline ComPtr<IFolderView2> view_at(HWND hit, bool desktop) {
    ComPtr<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING, IID_PPV_ARGS(&windows)))) {
      return {};
    }
    CoSetProxyBlanket(windows.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING);
    if (desktop) {
      VARIANT empty {};
      long hwnd {};
      ComPtr<IDispatch> dispatch;
      windows->FindWindowSW(&empty, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, &dispatch);
      return folder_view(dispatch.Get(), hit);
    }
    long count {};
    windows->get_Count(&count);
    for (long i = 0; i < count; ++i) {
      VARIANT index {};
      index.vt = VT_I4;
      index.lVal = i;
      ComPtr<IDispatch> dispatch;
      if (SUCCEEDED(windows->Item(index, &dispatch))) {
        if (auto view = folder_view(dispatch.Get(), hit)) {
          return view;
        }
      }
    }
    return {};
  }

  /** @brief Read a shell-owned display name or filesystem path. */
  inline std::wstring item_name(IShellItem *item, SIGDN kind) {
    PWSTR name {};
    if (!item || FAILED(item->GetDisplayName(kind, &name)) || !name) {
      return {};
    }
    std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> release(name, &CoTaskMemFree);
    return name;
  }

  /** @brief Distinguish an item, the list background, and unrelated controls such as headers. */
  inline bool hit_item(POINT point, std::wstring &name) {
    ComPtr<IUIAutomation> automation;
    ComPtr<IUIAutomationElement> element;
    ComPtr<IUIAutomationTreeWalker> walker;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&automation))) || FAILED(automation->ElementFromPoint(point, &element)) || FAILED(automation->get_ControlViewWalker(&walker))) {
      return false;
    }
    for (int i = 0; element && i < 12; ++i) {
      CONTROLTYPEID type {};
      if (FAILED(element->get_CurrentControlType(&type))) {
        return false;
      }
      if (type == UIA_ListItemControlTypeId || type == UIA_DataItemControlTypeId) {
        BSTR value {};
        if (FAILED(element->get_CurrentName(&value)) || !value) {
          return false;
        }
        name.assign(value, SysStringLen(value));
        SysFreeString(value);
        return !name.empty();
      }
      if (type == UIA_ListControlTypeId || type == UIA_DataGridControlTypeId) {
        return true;
      }
      if (type != UIA_TextControlTypeId && type != UIA_ImageControlTypeId && type != UIA_CustomControlTypeId && type != UIA_GroupControlTypeId && type != UIA_PaneControlTypeId) {
        return false;
      }
      ComPtr<IUIAutomationElement> parent;
      if (FAILED(walker->GetParentElement(element.Get(), &parent))) {
        return false;
      }
      element = std::move(parent);
    }
    return false;
  }

  /** @brief Get the selected source paths or one destination folder. An ambiguous hit is rejected. */
  inline std::vector<std::wstring> paths_at(POINT point, bool source, HANDLE user = nullptr) {
    const HWND hit = WindowFromPhysicalPoint(point);
    if (!shell_window(hit)) {
      return {};
    }
    GUITHREADINFO input {sizeof(input)};
    if (source && GetGUIThreadInfo(0, &input) && (input.flags & (GUI_INMOVESIZE | GUI_INMENUMODE))) {
      return {};
    }
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) {
      return {};
    }

    struct com_scope {
      HRESULT result;  ///< Balance successful COM initializations on this thread.

      ~com_scope() {
        if (SUCCEEDED(result)) {
          CoUninitialize();
        }
      }
    } scope {initialized};

    const auto root = window_class(GetAncestor(hit, GA_ROOT));
    const bool desktop = root == L"WorkerW" || root == L"Progman";
    auto view = view_at(hit, desktop);
    std::wstring name;
    if (!view || !hit_item(point, name) || WindowFromPhysicalPoint(point) != hit) {
      return {};
    }
    if (name.empty()) {
      if (source) {
        return {};
      }
      if (desktop) {
        PWSTR path {};
        if (FAILED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, user, &path))) {
          return {};
        }
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> release(path, &CoTaskMemFree);
        return {path};
      }
      ComPtr<IPersistFolder2> folder;
      PIDLIST_ABSOLUTE id {};
      if (FAILED(view->GetFolder(IID_PPV_ARGS(&folder))) || FAILED(folder->GetCurFolder(&id)) || !id) {
        return {};
      }
      std::unique_ptr<void, decltype(&CoTaskMemFree)> release(reinterpret_cast<void *>(id), &CoTaskMemFree);
      ComPtr<IShellItem> item;
      SHCreateItemFromIDList(id, IID_PPV_ARGS(&item));
      auto path = item_name(item.Get(), SIGDN_FILESYSPATH);
      return path.empty() ? std::vector<std::wstring> {} : std::vector<std::wstring> {std::move(path)};
    }

    std::vector<std::wstring> paths;
    ComPtr<IShellItemArray> selection;
    DWORD count {};
    if (source && (FAILED(view->GetSelection(FALSE, &selection)) || !selection || FAILED(selection->GetCount(&count)) || count > 4096)) {
      return {};
    }
    bool found = false;
    int visible = -1;
    for (DWORD i = 0; !source || i < count; ++i) {
      ComPtr<IShellItem> item;
      if (source) {
        if (FAILED(selection->GetItemAt(i, &item))) {
          return {};
        }
      } else {
        int next = -1;
        const auto hr = view->GetVisibleItem(visible, FALSE, &next);
        if (hr == S_FALSE) {
          break;
        }
        if (FAILED(hr) || next <= visible || FAILED(view->GetItem(next, IID_PPV_ARGS(&item)))) {
          return {};
        }
        visible = next;
      }
      const bool matches = item_name(item.Get(), SIGDN_NORMALDISPLAY) == name;
      if (matches && found) {
        return {};
      }
      found |= matches;
      if (source || matches) {
        auto path = item_name(item.Get(), SIGDN_FILESYSPATH);
        if (path.empty()) {
          return {};
        }
        if (!source) {
          const auto attrs = GetFileAttributesW(path.c_str());
          if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) || (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
            return {};
          }
        }
        paths.emplace_back(std::move(path));
      }
    }
    return found && WindowFromPhysicalPoint(point) == hit ? std::move(paths) : std::vector<std::wstring> {};
  }
}  // namespace shell_file_drop
