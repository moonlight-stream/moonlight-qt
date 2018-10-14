#include "antihookingprotection.h"

#include <NktHookLib.h>

typedef HMODULE (WINAPI *LoadLibraryAFunc)(LPCSTR lpLibFileName);
typedef HMODULE (WINAPI *LoadLibraryWFunc)(LPCWSTR lpLibFileName);
typedef HMODULE (WINAPI *LoadLibraryExAFunc)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE (WINAPI *LoadLibraryExWFunc)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

class AntiHookingProtection
{
public:
    static void enable()
    {
    #ifdef QT_DEBUG
        s_HookManager.SetEnableDebugOutput(true);
    #endif

        HINSTANCE kernel32Handle = NktHookLibHelpers::GetModuleBaseAddress(L"kernel32.dll");
        SIZE_T hookId;

        s_HookManager.Hook(&hookId, (LPVOID*)&s_RealLoadLibraryA,
                           NktHookLibHelpers::GetProcedureAddress(kernel32Handle, "LoadLibraryA"),
                           (LPVOID)AntiHookingProtection::LoadLibraryAHook);
        s_HookManager.Hook(&hookId, (LPVOID*)&s_RealLoadLibraryW,
                           NktHookLibHelpers::GetProcedureAddress(kernel32Handle, "LoadLibraryW"),
                           (LPVOID)AntiHookingProtection::LoadLibraryWHook);
        s_HookManager.Hook(&hookId, (LPVOID*)&s_RealLoadLibraryExA,
                           NktHookLibHelpers::GetProcedureAddress(kernel32Handle, "LoadLibraryExA"),
                           (LPVOID)AntiHookingProtection::LoadLibraryExAHook);
        s_HookManager.Hook(&hookId, (LPVOID*)&s_RealLoadLibraryExW,
                           NktHookLibHelpers::GetProcedureAddress(kernel32Handle, "LoadLibraryExW"),
                           (LPVOID)AntiHookingProtection::LoadLibraryExWHook);
    }

private:
    static bool isImageBlacklistedW(LPCWSTR lpLibFileName)
    {
        LPCWSTR dllName;

        // If the library has a path prefixed, remove it
        dllName = wcsrchr(lpLibFileName, '\\');
        if (!dllName) {
            // No prefix, so use the full name
            dllName = lpLibFileName;
        }
        else {
            // Advance past the backslash
            dllName++;
        }

        // FIXME: We don't currently handle LoadLibrary calls where the
        // library name does not include a file extension and the loader
        // automatically assumes .dll.

        for (int i = 0; i < ARRAYSIZE(k_BlacklistedDlls); i++) {
            if (_wcsicmp(dllName, k_BlacklistedDlls[i]) == 0) {
                return true;
            }
        }

        return false;
    }

    static bool isImageBlacklistedA(LPCSTR lpLibFileName)
    {
        int uniChars = MultiByteToWideChar(CP_THREAD_ACP, 0, lpLibFileName, -1, nullptr, 0);
        if (uniChars > 0) {
            PWCHAR wideBuffer = new WCHAR[uniChars];
            uniChars = MultiByteToWideChar(CP_THREAD_ACP, 0,
                                           lpLibFileName, -1,
                                           wideBuffer, uniChars * sizeof(WCHAR));
            if (uniChars > 0) {
                bool ret = isImageBlacklistedW(wideBuffer);
                delete[] wideBuffer;
                return ret;
            }
            else {
                delete[] wideBuffer;
            }
        }

        // Error path
        return false;
    }

    static HMODULE LoadLibraryAHook(LPCSTR lpLibFileName)
    {
        if (lpLibFileName && isImageBlacklistedA(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryA(lpLibFileName);
    }

    static HMODULE LoadLibraryWHook(LPCWSTR lpLibFileName)
    {
        if (lpLibFileName && isImageBlacklistedW(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryW(lpLibFileName);
    }

    static HMODULE LoadLibraryExAHook(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        if (lpLibFileName && isImageBlacklistedA(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryExA(lpLibFileName, hFile, dwFlags);
    }

    static HMODULE LoadLibraryExWHook(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        if (lpLibFileName && isImageBlacklistedW(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryExW(lpLibFileName, hFile, dwFlags);
    }

    static CNktHookLib s_HookManager;
    static LoadLibraryAFunc s_RealLoadLibraryA;
    static LoadLibraryWFunc s_RealLoadLibraryW;
    static LoadLibraryExAFunc s_RealLoadLibraryExA;
    static LoadLibraryExWFunc s_RealLoadLibraryExW;

    static constexpr LPCWSTR k_BlacklistedDlls[] = {
        // This DLL shipped with ASUS Sonic Radar 3 improperly handles
        // D3D9 exclusive fullscreen in a way that causes CreateDeviceEx()
        // to deadlock. https://github.com/moonlight-stream/moonlight-qt/issues/102
        L"NahimicOSD.dll"
    };
};

CNktHookLib AntiHookingProtection::s_HookManager;
LoadLibraryAFunc AntiHookingProtection::s_RealLoadLibraryA;
LoadLibraryWFunc AntiHookingProtection::s_RealLoadLibraryW;
LoadLibraryExAFunc AntiHookingProtection::s_RealLoadLibraryExA;
LoadLibraryExWFunc AntiHookingProtection::s_RealLoadLibraryExW;

AH_EXPORT void AntiHookingDummyImport() {}

extern "C"
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        AntiHookingProtection::enable();
        DisableThreadLibraryCalls(hinstDLL);
        break;
    }

    return TRUE;
};
