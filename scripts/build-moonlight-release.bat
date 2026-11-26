@echo off
setlocal
call "%~dp0find-vswhere.bat"
if errorlevel 1 exit /b 1
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VC_SETUP=%%i\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VC_SETUP%" (
    echo Visual Studio x64 C++ tools are required. 1>&2
    exit /b 1
)
call "%VC_SETUP%"
if errorlevel 1 exit /b 1
for %%i in ("%MOONLIGHT_QMAKE%") do set "PATH=%%~dpi;%PATH%"
set "CFLAGS=/GL"
set "CXXFLAGS=/GL /utf-8"
set "LDFLAGS=/LTCG"
"%MOONLIGHT_QMAKE%" "%~dp0..\moonlight-qt.pro"
if errorlevel 1 exit /b 1
set "MAKE_CMD=nmake.exe"
where /q jom.exe
if not errorlevel 1 set "MAKE_CMD=jom.exe"
if exist "jom\jom.exe" set "MAKE_CMD=jom\jom.exe"
"%MAKE_CMD%" release
exit /b %errorlevel%
