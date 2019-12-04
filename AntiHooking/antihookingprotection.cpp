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

    static HMODULE WINAPI LoadLibraryAHook(LPCSTR lpLibFileName)
    {
        if (lpLibFileName && isImageBlacklistedA(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryA(lpLibFileName);
    }

    static HMODULE WINAPI LoadLibraryWHook(LPCWSTR lpLibFileName)
    {
        if (lpLibFileName && isImageBlacklistedW(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryW(lpLibFileName);
    }

    static HMODULE WINAPI LoadLibraryExAHook(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        if (lpLibFileName && isImageBlacklistedA(lpLibFileName)) {
            SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
            return nullptr;
        }

        return s_RealLoadLibraryExA(lpLibFileName, hFile, dwFlags);
    }

    static HMODULE WINAPI LoadLibraryExWHook(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
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
        L"NahimicOSD.dll",

        // This DLL has been seen in several crash reports. Some Googling
        // suggests it's highly unstable and causes issues in many games.
        L"EZFRD32.dll",
        L"EZFRD64.dll",

        // These are the dList DLLs for Optimus hybrid graphics DDI.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/hybrid-system-ddi
        //
        // We forcefully block them from loading because Optimus has a bug that
        // deadlocks DXVA2 when we present with D3DPRESENT_DONOTWAIT. This will prevent
        // Optimus from ever using the dGPU even if the user has requested it.
        // https://github.com/moonlight-stream/moonlight-qt/issues/240
        // https://github.com/moonlight-stream/moonlight-qt/issues/235
        L"nvdlist.dll",
        L"nvdlistx.dll",

        // In some unknown circumstances, RTSS tries to hook in the middle of an instruction, leaving garbage
        // code inside d3d9.dll that causes a crash when executed:
        //
        // 0:000> u
        // d3d9!D3D9GetCurrentOwnershipMode+0x5d:
        // 00007ff8`95b95861 9b              wait
        // 00007ff8`95b95862 a7              cmps    dword ptr [rsi],dword ptr [rdi]   <--- crash happens here
        // 00007ff8`95b95863 ff              ???
        // 00007ff8`95b95864 bfe8ca8a00      mov     edi,8ACAE8h
        // 00007ff8`95b95869 00eb            add     bl,ch
        // 00007ff8`95b9586b f1              ???
        // 00007ff8`95b9586c b808000000      mov     eax,8
        // 00007ff8`95b95871 ebe6            jmp     d3d9!D3D9GetCurrentOwnershipMode+0x55 (00007ff8`95b95859)
        //
        // Disassembling starting at the exact address of the attempted hook yields the intended jmp instruction
        //
        // 0:000> u d3d9!D3D9GetCurrentOwnershipMode+0x5c:
        // 00007ff8`95b95860 e99ba7ffbf      jmp     00007ff8`55b90000
        //
        // Since the RTSS OSD doesn't even work with DXVA2, we'll just block the hooks entirely.
        L"RTSSHooks.dll",
        L"RTSSHooks64.dll",
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
