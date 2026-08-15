@echo off

rem Search the PATH first, then well-known paths
where /q vswhere.exe
if %ERRORLEVEL% EQU 0 (
    set VSWHERE=vswhere.exe
) else if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
) else if exist "%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set VSWHERE="%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
) else (
    exit /b 1
)

exit /b 0