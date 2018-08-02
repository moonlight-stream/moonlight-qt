rem Run from Qt command prompt with working directory set to root of repo

setlocal enableDelayedExpansion
@echo off

set BUILD_CONFIG=%1
set ARCH=%2

rem Convert to lower case for windeployqt
if /I "%BUILD_CONFIG%"=="debug" (
    set BUILD_CONFIG=debug
) else (
    if /I "%BUILD_CONFIG%"=="release" (
        set BUILD_CONFIG=release
    ) else (
        if /I "%BUILD_CONFIG%"=="signed-release" (
            set BUILD_CONFIG=release
            set SIGN=1
        ) else (
            echo Invalid build configuration
            exit /b 1
        )
    )
)

if /I "%ARCH%" NEQ "x86" (
    if /I "%ARCH%" NEQ "x64" (
        echo Invalid build architecture
        exit /b 1
    )
)

set VS_PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community
set SIGNTOOL_PARAMS=sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /sha1 1B3C676E831A94EC0327C3347EB32C68C26B3A67 /v

call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" %ARCH%
if !ERRORLEVEL! NEQ 0 goto Error

set BUILD_ROOT=%cd%\build
set SOURCE_ROOT=%cd%
set BUILD_FOLDER=%BUILD_ROOT%\build-%ARCH%-%BUILD_CONFIG%
set DEPLOY_FOLDER=%BUILD_ROOT%\deploy-%ARCH%-%BUILD_CONFIG%
set INSTALLER_FOLDER=%BUILD_ROOT%\installer-%ARCH%-%BUILD_CONFIG%

echo Cleaning output directories
rmdir /s /q %DEPLOY_FOLDER%
rmdir /s /q %BUILD_FOLDER%
rmdir /s /q %INSTALLER_FOLDER%
mkdir %BUILD_ROOT%
mkdir %DEPLOY_FOLDER%
mkdir %BUILD_FOLDER%
mkdir %INSTALLER_FOLDER%

echo Configuring the project
pushd %BUILD_FOLDER%
qmake %SOURCE_ROOT%\moonlight-qt.pro
if !ERRORLEVEL! NEQ 0 goto Error
popd

echo Compiling Moonlight in %BUILD_CONFIG% configuration
pushd %BUILD_FOLDER%
nmake %BUILD_CONFIG%
if !ERRORLEVEL! NEQ 0 goto Error
popd

echo Copying DLL dependencies
copy %SOURCE_ROOT%\libs\windows\lib\%ARCH%\*.dll %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

echo Deploying Qt dependencies
windeployqt.exe --dir %DEPLOY_FOLDER% --%BUILD_CONFIG% --qmldir %SOURCE_ROOT%\app\gui --no-opengl-sw --no-compiler-runtime %BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe
if !ERRORLEVEL! NEQ 0 goto Error

echo Harvesting files for WiX
"%WIX%\bin\heat" dir %DEPLOY_FOLDER% -srd -sfrag -ag -sw5150 -cg MoonlightDependencies -var var.SourceDir -dr INSTALLFOLDER -out %BUILD_FOLDER%\Dependencies.wxs
if !ERRORLEVEL! NEQ 0 goto Error

echo Copying application binary to deployment directory
copy %BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

if "%SIGN%"=="1" (
    echo Signing deployed binaries
    for /r "%DEPLOY_FOLDER%" %%f in (*.dll *.exe) do (
        signtool %SIGNTOOL_PARAMS% %%f
        if !ERRORLEVEL! NEQ 0 goto Error
    )
)

echo Building installer
set VCREDIST_INSTALLER=%VS_PATH%\VC\Redist\MSVC\14.14.26405\vcredist_%ARCH%.exe
msbuild %SOURCE_ROOT%\wix\Moonlight.sln /p:Configuration=%BUILD_CONFIG% /p:Platform=%ARCH%
if !ERRORLEVEL! NEQ 0 goto Error

if "%SIGN%"=="1" (
    echo Signing installer binary
    "%WIX%\bin\insignia" -ib %INSTALLER_FOLDER%\MoonlightSetup.exe -o %BUILD_FOLDER%\engine.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    signtool %SIGNTOOL_PARAMS% %BUILD_FOLDER%\engine.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    "%WIX%\bin\insignia" -ab %BUILD_FOLDER%\engine.exe %INSTALLER_FOLDER%\MoonlightSetup.exe -o %INSTALLER_FOLDER%\MoonlightSetup.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    signtool %SIGNTOOL_PARAMS% %INSTALLER_FOLDER%\MoonlightSetup.exe
    if !ERRORLEVEL! NEQ 0 goto Error
)

if /i "%APPVEYOR%"=="true" (
    echo Pushing artifacts
    appveyor PushArtifact %INSTALLER_FOLDER%\MoonlightSetup.exe -FileName MoonlightSetup-%ARCH%-%BUILD_CONFIG%.exe
    if !ERRORLEVEL! NEQ 0 goto Error
)

echo Build successful!
exit /b 0

:Error
echo Build failed!
exit /b !ERRORLEVEL!