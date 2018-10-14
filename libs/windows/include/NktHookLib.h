/*
 * Copyright (C) 2010-2013 Nektra S.A., Buenos Aires, Argentina.
 * All rights reserved. Contact: http://www.nektra.com
 *
 *
 * This file is part of Deviare In-Proc
 *
 *
 * Commercial License Usage
 * ------------------------
 * Licensees holding valid commercial Deviare In-Proc licenses may use this
 * file in accordance with the commercial license agreement provided with the
 * Software or, alternatively, in accordance with the terms contained in
 * a written agreement between you and Nektra.  For licensing terms and
 * conditions see http://www.nektra.com/licensing/.  For further information
 * use the contact form at http://www.nektra.com/contact/.
 *
 *
 * GNU General Public License Usage
 * --------------------------------
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl.html.
 *
 **/

#ifndef _NKTHOOKLIB
#define _NKTHOOKLIB

#include <windows.h>

//-----------------------------------------------------------

#define NKTHOOKLIB_DontSkipInitialJumps               0x0001
#define NKTHOOKLIB_DontRemoveOnUnhook                 0x0002
#define NKTHOOKLIB_DontSkipAnyJumps                   0x0004
#define NKTHOOKLIB_SkipNullProcsToHook                0x0008
#define NKTHOOKLIB_UseAbsoluteIndirectJumps           0x0010
#define NKTHOOKLIB_DisallowReentrancy                 0x0020
#define NKTHOOKLIB_DontEnableHooks                    0x0040

#define NKTHOOKLIB_ProcessPlatformX86                      1
#define NKTHOOKLIB_ProcessPlatformX64                      2

#define NKTHOOKLIB_CurrentProcess     ((HANDLE)(LONG_PTR)-1)
#define NKTHOOKLIB_CurrentThread      ((HANDLE)(LONG_PTR)-2)

//-----------------------------------------------------------

class CNktHookLib
{
public:
  typedef struct tagHOOK_INFO {
    SIZE_T nHookId;
    LPVOID lpProcToHook;
    LPVOID lpNewProcAddr;
    //----
    LPVOID lpCallOriginal;
  } HOOK_INFO, *LPHOOK_INFO;

  CNktHookLib();
  ~CNktHookLib();

  DWORD Hook(__out SIZE_T *lpnHookId, __out LPVOID *lplpCallOriginal, __in LPVOID lpProcToHook,
             __in LPVOID lpNewProcAddr, __in DWORD dwFlags=0);
  DWORD Hook(__inout HOOK_INFO aHookInfo[], __in SIZE_T nCount, __in DWORD dwFlags=0);
  DWORD Hook(__inout LPHOOK_INFO aHookInfo[], __in SIZE_T nCount, __in DWORD dwFlags = 0);

  DWORD RemoteHook(__out SIZE_T *lpnHookId, __out LPVOID *lplpCallOriginal, __in DWORD dwPid,
                   __in LPVOID lpProcToHook, __in LPVOID lpNewProcAddr, __in DWORD dwFlags);
  DWORD RemoteHook(__inout HOOK_INFO aHookInfo[], __in SIZE_T nCount, __in DWORD dwPid, __in DWORD dwFlags);
  DWORD RemoteHook(__inout LPHOOK_INFO aHookInfo[], __in SIZE_T nCount, __in DWORD dwPid, __in DWORD dwFlags);

  DWORD RemoteHook(__out SIZE_T *lpnHookId, __out LPVOID *lplpCallOriginal, __in HANDLE hProcess,
                   __in LPVOID lpProcToHook, __in LPVOID lpNewProcAddr, __in DWORD dwFlags);
  DWORD RemoteHook(__inout HOOK_INFO aHookInfo[], __in SIZE_T nCount, __in HANDLE hProcess, __in DWORD dwFlags);
  DWORD RemoteHook(__inout LPHOOK_INFO aHookInfo[], __in SIZE_T nCount, __in HANDLE hProcess, __in DWORD dwFlags);

  DWORD Unhook(__in SIZE_T nHookId);
  DWORD Unhook(__in HOOK_INFO aHookInfo[], __in SIZE_T nCount);
  DWORD Unhook(__in LPHOOK_INFO aHookInfo[], __in SIZE_T nCount);
  VOID UnhookProcess(__in DWORD dwPid);
  VOID UnhookAll();

  //NOTE: The following 2 (two) methods will remove the hooks from the internal list of hooks but the original
  //      hook(s) will remain active.
  DWORD RemoveHook(__in SIZE_T nHookId, BOOL bDisable);
  DWORD RemoveHook(__in HOOK_INFO aHookInfo[], __in SIZE_T nCount, __in BOOL bDisable);
  DWORD RemoveHook(__in LPHOOK_INFO aHookInfo[], __in SIZE_T nCount, __in BOOL bDisable);

  DWORD EnableHook(__in SIZE_T nHookId, __in BOOL bEnable);
  DWORD EnableHook(__in HOOK_INFO aHookInfo[], __in SIZE_T nCount, __in BOOL bEnable);
  DWORD EnableHook(__in LPHOOK_INFO aHookInfo[], __in SIZE_T nCount, __in BOOL bEnable);

  DWORD SetSuspendThreadsWhileHooking(__in BOOL bEnable);
  BOOL GetSuspendThreadsWhileHooking();

  DWORD SetEnableDebugOutput(__in BOOL bEnable);
  BOOL GetEnableDebugOutput();

  void* __cdecl operator new(__in size_t nSize);
  void* __cdecl operator new[](__in size_t nSize);
  void* __cdecl operator new(__in size_t nSize, __inout void* lpInPlace);
  void __cdecl operator delete(__inout void* p);
  void __cdecl operator delete[](__inout void* p);
#if _MSC_VER >= 1200
  void __cdecl operator delete(__inout void* p, __inout void* lpPlace);
#endif //_MSC_VER >= 1200

private:
  DWORD HookCommon(__in LPVOID lpInfo, __in SIZE_T nCount, __in DWORD dwPid, __in DWORD dwFlags);
  DWORD UnhookCommon(__in LPVOID lpInfo, __in SIZE_T nCount, __in DWORD dwFlags);
  DWORD RemoveHookCommon(__in LPVOID lpInfo, __in SIZE_T nCount, __in BOOL bDisable, __in DWORD dwFlags);
  DWORD EnableHookCommon(__in LPVOID lpInfo, __in SIZE_T nCount, __in BOOL bEnable, __in DWORD dwFlags);

private:
  LPVOID lpInternals;
};

//-----------------------------------------------------------

namespace NktHookLibHelpers {

//NOTE: See "BuildNtSysCalls" below
typedef struct tagSYSCALLDEF {
  LPSTR szNtApiNameA;
  SIZE_T nOffset;
} SYSCALLDEF, *LPSYSCALLDEF;

//--------------------------------

//NOTE: See "SetApiResolverCallback" below
typedef LPVOID (__stdcall *lpfnInternalApiResolver)(__in_z LPCSTR szApiNameA, __in LPVOID lpUserParam);

//--------------------------------

HINSTANCE GetModuleBaseAddress(__in_z LPCWSTR szDllNameW);
LPVOID GetProcedureAddress(__in HINSTANCE hDll, __in LPCSTR szProcNameA);

HINSTANCE GetRemoteModuleBaseAddress(__in HANDLE hProcess, __in_z LPCWSTR szDllNameW, __in BOOL bScanMappedImages);
LPVOID GetRemoteProcedureAddress(__in HANDLE hProcess, __in HINSTANCE hDll, __in_z LPCSTR szProcNameA);

//--------------------------------

int sprintf_s(__out_z char *lpDest, __in size_t nMaxCount, __in_z const char *szFormatA, ...);
int vsnprintf(__out_z char *lpDest, __in size_t nMaxCount, __in_z const char *szFormatA, __in va_list lpArgList);

//only on XP or later
int swprintf_s(__out_z wchar_t *lpDest, __in size_t nMaxCount, __in_z const wchar_t *szFormatW, ...);
int vsnwprintf(__out_z wchar_t *lpDest, __in size_t nMaxCount, __in_z const wchar_t *szFormatW, __in va_list lpArgList);

//--------------------------------

//Returns a PROCESSOR_ARCHITECTURE_xxx value or -1 on error.
LONG GetProcessorArchitecture();

HANDLE OpenProcess(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in DWORD dwProcessId);
HANDLE OpenThread(__in DWORD dwDesiredAccess, __in BOOL bInheritHandle, __in DWORD dwThreadId);

LONG GetProcessPlatform(__in HANDLE hProcess);

SIZE_T ReadMem(__in HANDLE hProcess, __out LPVOID lpDest, __in LPVOID lpSrc, __in SIZE_T nBytesCount);
BOOL WriteMem(__in HANDLE hProcess, __out LPVOID lpDest, __in LPVOID lpSrc, __in SIZE_T nBytesCount);

LONG GetThreadPriority(__in HANDLE hThread, __out int *lpnPriority);
LONG SetThreadPriority(__in HANDLE hThread, __in int nPriority);

DWORD GetCurrentThreadId();
DWORD GetCurrentProcessId();

HANDLE GetProcessHeap();
LPVOID MemAlloc(__in SIZE_T nSize);
VOID MemFree(__in LPVOID lpPtr);

VOID MemSet(__out void *lpDest, __in int nVal, __in SIZE_T nCount);
VOID MemCopy(__out void *lpDest, __in const void *lpSrc, __in SIZE_T nCount);
SIZE_T TryMemCopy(__out void *lpDest, __in const void *lpSrc, __in SIZE_T nCount);
VOID MemMove(__out void *lpDest, __in const void *lpSrc, __in SIZE_T nCount);
int MemCompare(__in const void *lpBuf1, __in const void *lpBuf2, __in SIZE_T nCount);

//--------------------------------

VOID DebugPrint(__in LPCSTR szFormatA, ...);
VOID DebugVPrint(__in LPCSTR szFormatA, __in va_list argptr);

//--------------------------------

SIZE_T GetInstructionLength(__in LPVOID lpAddr, __in SIZE_T nSize, __in BYTE nPlatformBits,
                            __out_opt BOOL *lpbIsMemOp=NULL, __out_z_opt LPSTR szBufA=NULL, __in SIZE_T nBufLen=0);

//NOTE: When NktHookLib is initialized, it tries to locate needed ntdll's apis by scanning process' modules.
//      If you want to override an api call, use this method to set the resolver address. LPVOID returned by
//      the callback must have the same definition and calling convention than the one in ntdll. Return NULL
//      if you want NktHookLib to use the real ntdll api.
VOID SetInternalApiResolverCallback(__in lpfnInternalApiResolver fnInternalApiResolver, __in LPVOID lpUserParam);

//This function generates a relocatable byte code with a copy of the original SysCall routine for each passes
//ntdll api. The code is created based on the original ntdll.dll image file on disk located on System32 or SysWow64,
//depending on the target platform.
//
//Useful for doing direct calls to low level apis bypassing third party hooks (like Chrome navigator does). Also, you
//can pass the generated code to another process. You can generate the code with this method, pass it to another
//process and then use SetApiResolverCallback above.
//
//NOTE: Not all NtXXX apis are SysCalls. Trying to generate code for a non-syscall api may generate an unexpected
//      behavior.
//      If 'lpCode' is NULL, the needed space is returned. Although syscalls uses less than 32 bytes, a maximum of 256
//      bytes are supported for each requested api. You can safety allocate a block of 256*nDefsCount bytes to hold
//      the generated code.
DWORD BuildNtSysCalls(__in LPSYSCALLDEF lpDefs, __in SIZE_T nDefsCount, __in SIZE_T nPlatform,
                      __out_opt LPVOID lpCode, __out SIZE_T *lpnCodeSize);

//--------------------------------

//NOTE: Return 0xFFFFFFFF if remote thread is not accessible
DWORD GetWin32LastError(__in_opt HANDLE hThread=NULL);
BOOL SetWin32LastError(__in DWORD dwErrorCode, __in_opt HANDLE hThread=NULL);

//--------------------------------

BOOL GetOsVersion(__out_opt LPDWORD lpdwVerMajor=NULL, __out_opt LPDWORD lpdwVerMinor=NULL,
                  __out_opt LPDWORD lpdwBuildNumber=NULL);

//--------------------------------

//NOTE: CreateProcessWithDllW and related functions returns the Win32 error code directly. NOERROR => Success.
//
//      If "szDllNameW" string ends with 'x86.dll', 'x64.dll', '32.dll', '64.dll', the dll name will be adjusted
//      in order to match the process platform. I.e.: "mydll_x86.dll" will become "mydll_x64.dll" on 64-bit processes.
DWORD CreateProcessWithDllW(__in_z_opt LPCWSTR lpApplicationName, __inout_z_opt LPWSTR lpCommandLine,
                            __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
                            __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
                            __in DWORD dwCreationFlags, __in_z_opt LPCWSTR lpEnvironment,
                            __in_z_opt LPCWSTR lpCurrentDirectory, __in LPSTARTUPINFOW lpStartupInfo,
                            __out LPPROCESS_INFORMATION lpProcessInformation, __in_z LPCWSTR szDllNameW,
                            __in_opt HANDLE hSignalCompleted=NULL, __in_z_opt LPCSTR szInitFunctionA=NULL,
                            __in_opt LPVOID lpInitFuncParams=NULL, __in_opt ULONG nInitFuncParamsSize=0);

DWORD CreateProcessWithLogonAndDllW(__in_z LPCWSTR lpUsername, __in_z_opt LPCWSTR lpDomain, __in_z LPCWSTR lpPassword,
                                    __in DWORD dwLogonFlags, __in_opt LPCWSTR lpApplicationName,
                                    __inout_opt LPWSTR lpCommandLine, __in DWORD dwCreationFlags,
                                    __in_z_opt LPCWSTR lpEnvironment, __in_z_opt LPCWSTR lpCurrentDirectory,
                                    __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation,
                                    __in_z LPCWSTR szDllNameW, __in_opt HANDLE hSignalCompleted=NULL,
                                    __in_z_opt LPCSTR szInitFunctionA=NULL, __in_opt LPVOID lpInitFuncParams=NULL,
                                    __in_opt ULONG nInitFuncParamsSize=0);

DWORD CreateProcessWithTokenAndDllW(__in HANDLE hToken, __in DWORD dwLogonFlags, __in_z_opt LPCWSTR lpApplicationName,
                                    __inout_opt LPWSTR lpCommandLine, __in DWORD dwCreationFlags,
                                    __in_z_opt LPCWSTR lpEnvironment, __in_z_opt LPCWSTR lpCurrentDirectory,
                                    __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation,
                                    __in_z LPCWSTR szDllNameW, __in_opt HANDLE hSignalCompleted=NULL,
                                    __in_z_opt LPCSTR szInitFunctionA=NULL, __in_opt LPVOID lpInitFuncParams=NULL,
                                    __in_opt ULONG nInitFuncParamsSize=0);

DWORD InjectDllByPidW(__in DWORD dwPid, __in_z LPCWSTR szDllNameW, __in_z_opt LPCSTR szInitFunctionA=NULL,
                      __in_opt DWORD dwProcessInitWaitTimeoutMs=5000, __out_opt LPHANDLE lphInjectorThread=NULL,
                      __in_opt LPVOID lpInitFuncParams=NULL, __in_opt ULONG nInitFuncParamsSize=0);

DWORD InjectDllByHandleW(__in HANDLE hProcess, __in_z LPCWSTR szDllNameW, __in_z_opt LPCSTR szInitFunctionA=NULL,
                         __in_opt DWORD dwProcessInitWaitTimeoutMs=5000, __out_opt LPHANDLE lphInjectorThread=NULL,
                         __in_opt LPVOID lpInitFuncParams=NULL, __in_opt ULONG nInitFuncParamsSize=0);

} //NktHookLibHelpers

//-----------------------------------------------------------

#endif //_NKTHOOKLIB
