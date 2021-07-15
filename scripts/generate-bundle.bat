@echo off
setlocal enableDelayedExpansion

rem Run from Qt command prompt with working directory set to root of repo

set BUILD_CONFIG=%1

if "%INCLUDE_ARM64%" EQU "" (
    set INCLUDE_ARM64=0
)

rem Convert to lower case for windeployqt
if /I "%BUILD_CONFIG%"=="debug" (
    set BUILD_CONFIG=debug
    set WIX_MUMS=10
) else (
    if /I "%BUILD_CONFIG%"=="release" (
        set BUILD_CONFIG=release
        set WIX_MUMS=10
    ) else (
        if /I "%BUILD_CONFIG%"=="signed-release" (
            set BUILD_CONFIG=release
            set SIGN=1
            set MUST_DEPLOY_SYMBOLS=1

            rem Release installers must have ARM64
            set INCLUDE_ARM64=1

            rem Fail if there are unstaged changes
            git diff-index --quiet HEAD --
            if !ERRORLEVEL! NEQ 0 (
                echo Signed release builds must not have unstaged changes!
                exit /b 1
            )
        ) else (
            echo Invalid build configuration - expected 'debug' or 'release'
            exit /b 1
        )
    )
)

set SIGNTOOL_PARAMS=sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /sha1 b28642b756ebec4884d1063dfa4de803a6dcecdc /v

set BUILD_ROOT=%cd%\build
set SOURCE_ROOT=%cd%
set BUILD_FOLDER=%BUILD_ROOT%\build-%BUILD_CONFIG%
set INSTALLER_FOLDER=%BUILD_ROOT%\installer-%BUILD_CONFIG%
set /p VERSION=<%SOURCE_ROOT%\app\version.txt

rem Ensure that all architectures have been built before the final bundle
if not exist "%BUILD_ROOT%\build-x86-%BUILD_CONFIG%\Moonlight.msi" (
    echo Unable to build bundle - missing binaries for %BUILD_CONFIG% x86
    echo You must run 'build-arch.bat %BUILD_CONFIG% x86' first
    exit /b 1
)
if not exist "%BUILD_ROOT%\build-x64-%BUILD_CONFIG%\Moonlight.msi" (
    echo Unable to build bundle - missing binaries for %BUILD_CONFIG% x64
    echo You must run 'build-arch.bat %BUILD_CONFIG% x64' first
    exit /b 1
)
if %INCLUDE_ARM64% NEQ 0 (
    if not exist "%BUILD_ROOT%\build-arm64-%BUILD_CONFIG%\Moonlight.msi" (
        echo Unable to build bundle - missing binaries for %BUILD_CONFIG% arm64
        echo You must run 'build-arch.bat %BUILD_CONFIG% arm64' first
        exit /b 1
    )
)

echo Cleaning output directories
rmdir /s /q %BUILD_FOLDER%
rmdir /s /q %INSTALLER_FOLDER%
mkdir %BUILD_FOLDER%
mkdir %INSTALLER_FOLDER%

rem Find Visual Studio and run vcvarsall.bat
set VSWHERE="%SOURCE_ROOT%\scripts\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do (
    call "%%i\VC\Auxiliary\Build\vcvarsall.bat" x86
)
if !ERRORLEVEL! NEQ 0 goto Error

echo Building bundle
rem Bundles are always x86 binaries
msbuild %SOURCE_ROOT%\wix\MoonlightSetup\MoonlightSetup.wixproj /p:Configuration=%BUILD_CONFIG% /p:Platform=x86 /p:DefineConstants="INCLUDE_ARM64=%INCLUDE_ARM64%"
if !ERRORLEVEL! NEQ 0 goto Error

if "%SIGN%"=="1" (
    echo Signing bundle
    "%WIX%\bin\insignia" -ib %INSTALLER_FOLDER%\MoonlightSetup.exe -o %BUILD_FOLDER%\engine.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    signtool %SIGNTOOL_PARAMS% %BUILD_FOLDER%\engine.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    "%WIX%\bin\insignia" -ab %BUILD_FOLDER%\engine.exe %INSTALLER_FOLDER%\MoonlightSetup.exe -o %INSTALLER_FOLDER%\MoonlightSetup.exe
    if !ERRORLEVEL! NEQ 0 goto Error
    signtool %SIGNTOOL_PARAMS% %INSTALLER_FOLDER%\MoonlightSetup.exe
    if !ERRORLEVEL! NEQ 0 goto Error
)

rem Rename the installer to match the publishing convention
ren %INSTALLER_FOLDER%\MoonlightSetup.exe MoonlightSetup-%VERSION%.exe

echo Build successful for Moonlight v%VERSION% installer!
echo.
echo x86 included: YES
echo x64 included: YES
if %INCLUDE_ARM64% NEQ 0 (
    echo ARM64 included: YES
) else (
    echo ARM64 included: NO
)
exit /b 0

:Error
echo Build failed!
exit /b !ERRORLEVEL!
