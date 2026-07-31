@echo off
setlocal enableDelayedExpansion

rem Run from Qt command prompt with working directory set to root of repo

set BUILD_CONFIG=%1
set BUILD_ARCH=%2

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

            rem Fail if there are unstaged changes
            git diff-index --quiet HEAD --
            if !ERRORLEVEL! NEQ 0 (
                echo Signed release builds must not have unstaged changes!
                exit /b 1
            )

            echo Updating dependencies
            powershell %cd%\setup-deps.ps1
            if !ERRORLEVEL! NEQ 0 (
                exit /b 1
            )
        ) else (
            echo Invalid build configuration - expected 'debug' or 'release'
            echo Usage: scripts\build-arch.bat ^(release^|debug^)
            exit /b 1
        )
    )
)

rem Locate qmake and determine if we're using qmake.exe or (host-)qmake.bat
rem (host-)qmake.bat is an ARM64 forwarder to the x64 version of qmake.exe
where qmake.bat
if !ERRORLEVEL! EQU 0 (
    set QMAKE_CMD=call qmake.bat
) else (
    where host-qmake.bat
    if !ERRORLEVEL! EQU 0 (
        set QMAKE_CMD=call host-qmake.bat
    ) else (
        where qmake.exe
        if !ERRORLEVEL! EQU 0 (
            set QMAKE_CMD=qmake.exe
        ) else (
            echo Unable to find QMake. Did you add Qt bins to your PATH?
            goto Error
        )
    )
)

rem Find Qt path to determine our architecture
set QT_PATH=
if /I "%QMAKE_CMD%"=="call qmake.bat" (
    for /F "delims=" %%i in ('where qmake.bat') do if not defined QT_PATH set "QT_PATH=%%i"
) else (
    if /I "%QMAKE_CMD%"=="call host-qmake.bat" (
        for /F "delims=" %%i in ('where host-qmake.bat') do if not defined QT_PATH set "QT_PATH=%%i"
    ) else (
        for /F "delims=" %%i in ('where qmake.exe') do if not defined QT_PATH set "QT_PATH=%%i"
    )
)
if not defined QT_PATH (
    echo Unable to resolve QMake path.
    goto Error
)

rem Strip the qmake filename off the end to get the Qt bin directory itself
set QT_PATH=%QT_PATH:\qmake.exe=%
set QT_PATH=%QT_PATH:\qmake.bat=%
set QT_PATH=%QT_PATH:\host-qmake.bat=%
set QT_PATH=%QT_PATH:\qmake.cmd=%

echo QT_PATH=%QT_PATH%
if /I "%BUILD_ARCH%"=="arm64" (
    set ARCH=arm64

    rem Replace the _arm64 suffix with _64 to get the x64 bin path
    set HOSTBIN_PATH=%QT_PATH:_arm64=_64%
    echo HOSTBIN_PATH=!HOSTBIN_PATH!

    if exist %QT_PATH%\host-qtpaths.bat (
        echo Using windeployqt.exe from HOSTBIN_PATH
        set WINDEPLOYQT_CMD=!HOSTBIN_PATH!\windeployqt.exe --qtpaths %QT_PATH%\host-qtpaths.bat
    ) else (
        if exist %QT_PATH%\qtpaths.bat (
            echo Using windeployqt.exe from HOSTBIN_PATH
            set WINDEPLOYQT_CMD=!HOSTBIN_PATH!\windeployqt.exe --qtpaths %QT_PATH%\qtpaths.bat
        ) else (
            echo Using windeployqt.exe from QT_PATH
            set WINDEPLOYQT_CMD=%QT_PATH%\windeployqt.exe
        )
    )
) else (
    set ARCH=x64
    set WINDEPLOYQT_CMD=windeployqt.exe
)

echo Detected target architecture: %ARCH%

set SIGNTOOL_PARAMS=sign /tr http://timestamp.digicert.com /td sha256 /fd sha256 /sha1 8b9d0d682ad9459e54f05a79694bc10f9876e297 /v

set BUILD_ROOT=%cd%\build
set SOURCE_ROOT=%cd%
set BUILD_FOLDER=%BUILD_ROOT%\build-%ARCH%-%BUILD_CONFIG%
set DEPLOY_FOLDER=%BUILD_ROOT%\deploy-%ARCH%-%BUILD_CONFIG%
set INSTALLER_FOLDER=%BUILD_ROOT%\installer-%ARCH%-%BUILD_CONFIG%
set SYMBOLS_FOLDER=%BUILD_ROOT%\symbols-%ARCH%-%BUILD_CONFIG%

rem Derive artifact version from CI_VERSION or Git tags (fallback: app\version.txt)
for /f "usebackq delims=" %%i in (`python "%SOURCE_ROOT%\scripts\derive-version.py" --source-root "%SOURCE_ROOT%" --field artifact`) do set VERSION=%%i

rem Use the correct VC tools for the specified architecture
if /I "%ARCH%" EQU "x64" (
    rem x64 is a special case that doesn't match %PROCESSOR_ARCHITECTURE%
    set VC_ARCH=AMD64
) else (
    set VC_ARCH=%ARCH%
)

rem If we're not building for the current platform, use the cross compiling toolchain
if /I "%VC_ARCH%" NEQ "%PROCESSOR_ARCHITECTURE%" (
    set VC_ARCH=%PROCESSOR_ARCHITECTURE%_%VC_ARCH%
)

rem Find Visual Studio and run vcvarsall.bat
set VSWHERE="%SOURCE_ROOT%\scripts\vswhere.exe"
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -property installationPath`) do (
    call "%%i\VC\Auxiliary\Build\vcvarsall.bat" %VC_ARCH%
)
if !ERRORLEVEL! NEQ 0 goto Error

rem Find VC redistributable DLLs
for /f "usebackq delims=" %%i in (`%VSWHERE% -latest -find VC\Redist\MSVC\*\%ARCH%\Microsoft.VC*.CRT`) do set VC_REDIST_DLL_PATH=%%i

echo Cleaning output directories
rmdir /s /q %DEPLOY_FOLDER%
rmdir /s /q %BUILD_FOLDER%
rmdir /s /q %INSTALLER_FOLDER%
rmdir /s /q %SYMBOLS_FOLDER%
if not exist %BUILD_ROOT% mkdir %BUILD_ROOT%
mkdir %DEPLOY_FOLDER%
mkdir %BUILD_FOLDER%
mkdir %INSTALLER_FOLDER%
mkdir %SYMBOLS_FOLDER%

rem Enable LTCG for official builds
set CFLAGS=/GL
set CXXFLAGS=/GL
set LDFLAGS=/LTCG

echo Configuring the project
pushd %BUILD_FOLDER%
%QMAKE_CMD% %SOURCE_ROOT%\moonlight-qt.pro
if !ERRORLEVEL! NEQ 0 goto Error
popd

echo Compiling Moonlight in %BUILD_CONFIG% configuration
pushd %BUILD_FOLDER%
%SOURCE_ROOT%\scripts\jom.exe %BUILD_CONFIG%
if !ERRORLEVEL! NEQ 0 goto Error
popd

echo Saving PDBs
for /r "%BUILD_FOLDER%" %%f in (*.pdb) do (
    copy "%%f" %SYMBOLS_FOLDER%
    if !ERRORLEVEL! NEQ 0 goto Error
)
copy %SOURCE_ROOT%\libs\windows\lib\%ARCH%\*.pdb %SYMBOLS_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error
7z a %SYMBOLS_FOLDER%\MoonlightDebuggingSymbols-%ARCH%-%VERSION%.zip %SYMBOLS_FOLDER%\*.pdb
if !ERRORLEVEL! NEQ 0 goto Error

if "%ML_SYMBOL_STORE%" NEQ "" (
    echo Publishing PDBs to symbol store: %ML_SYMBOL_STORE%
    symstore add /f %SYMBOLS_FOLDER%\*.pdb /s %ML_SYMBOL_STORE% /t Moonlight
    if !ERRORLEVEL! NEQ 0 goto Error
) else (
    if "%MUST_DEPLOY_SYMBOLS%"=="1" (
        echo "A symbol server must be specified in ML_SYMBOL_STORE for signed release builds"
        exit /b 1
    )
)

if "%ML_SYMBOL_ARCHIVE%" NEQ "" (
    echo Copying PDB ZIP to symbol archive: %ML_SYMBOL_ARCHIVE%
    copy %SYMBOLS_FOLDER%\MoonlightDebuggingSymbols-%ARCH%-%VERSION%.zip %ML_SYMBOL_ARCHIVE%
    if !ERRORLEVEL! NEQ 0 goto Error
) else (
    if "%MUST_DEPLOY_SYMBOLS%"=="1" (
        echo "A symbol archive directory must be specified in ML_SYMBOL_ARCHIVE for signed release builds"
        exit /b 1
    )
)

echo Copying DLL dependencies
copy %SOURCE_ROOT%\libs\windows\lib\%ARCH%\*.dll %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

echo Copying AntiHooking.dll
copy %BUILD_FOLDER%\AntiHooking\%BUILD_CONFIG%\AntiHooking.dll %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

echo Copying clipboard helper
copy %BUILD_FOLDER%\clipboard-helper\%BUILD_CONFIG%\moonlight-clipboard-helper.exe %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

echo Copying GC mapping list
copy %SOURCE_ROOT%\app\SDL_GameControllerDB\gamecontrollerdb.txt %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

if not x%QT_PATH:\5.=%==x%QT_PATH% (
    echo Copying qt.conf for Qt 5
    copy %SOURCE_ROOT%\app\qt_qt5.conf %DEPLOY_FOLDER%\qt.conf
    if !ERRORLEVEL! NEQ 0 goto Error

    rem Qt 5.15
    set WINDEPLOYQT_ARGS=--no-qmltooling --no-virtualkeyboard
) else (
    rem Qt 6.8+
    set WINDEPLOYQT_ARGS=--no-system-d3d-compiler --no-system-dxc-compiler --skip-plugin-types qmltooling,generic --no-ffmpeg
    rem FluentWinUI3 depends on the Fusion fallback style, so deploy both styles
    rem and their implementation libraries.
    set WINDEPLOYQT_ARGS=!WINDEPLOYQT_ARGS! --no-quickcontrols2imagine --no-quickcontrols2universal
    set WINDEPLOYQT_ARGS=!WINDEPLOYQT_ARGS! --no-quickcontrols2imaginestyleimpl --no-quickcontrols2universalstyleimpl --no-quickcontrols2windowsstyleimpl
)

echo Deploying Qt dependencies
%WINDEPLOYQT_CMD% --dir %DEPLOY_FOLDER% --%BUILD_CONFIG% --qmldir %SOURCE_ROOT%\app\gui --no-opengl-sw --no-compiler-runtime --no-sql %WINDEPLOYQT_ARGS% %BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe
if !ERRORLEVEL! NEQ 0 goto Error

echo Deleting unused files
rem Qt 5.x directories
rmdir /s /q %DEPLOY_FOLDER%\QtQuick\Controls.2\Fusion
rmdir /s /q %DEPLOY_FOLDER%\QtQuick\Controls.2\Imagine
rmdir /s /q %DEPLOY_FOLDER%\QtQuick\Controls.2\Universal
rem Qt 6.8+ directories. FluentWinUI3 is the active style and imports
rem Fusion as a fallback, so both directories must remain in the bundle.
rmdir /s /q %DEPLOY_FOLDER%\qml\QtQuick\Controls\Imagine
rmdir /s /q %DEPLOY_FOLDER%\qml\QtQuick\Controls\Universal
rmdir /s /q %DEPLOY_FOLDER%\qml\QtQuick\Controls\Windows
rmdir /s /q %DEPLOY_FOLDER%\qml\QtQuick\NativeStyle
rem icuuc.dll ships with all supported OSes (and Qt incorrectly deploys the x64 version on ARM64)
del %DEPLOY_FOLDER%\icuuc.dll

if "%SIGN%"=="1" (
    echo Signing deployed binaries
    set FILES_TO_SIGN=%BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe
    for /r "%DEPLOY_FOLDER%" %%f in (*.dll *.exe) do (
        set FILES_TO_SIGN=!FILES_TO_SIGN! %%f
    )
    signtool %SIGNTOOL_PARAMS% !FILES_TO_SIGN!
    if !ERRORLEVEL! NEQ 0 goto Error
)

if "%ML_SYMBOL_STORE%" NEQ "" (
    echo Publishing binaries to symbol store: %ML_SYMBOL_STORE%
    symstore add /r /f %DEPLOY_FOLDER%\*.* /s %ML_SYMBOL_STORE% /t Moonlight
    if !ERRORLEVEL! NEQ 0 goto Error
    symstore add /r /f %BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe /s %ML_SYMBOL_STORE% /t Moonlight
    if !ERRORLEVEL! NEQ 0 goto Error
)

echo Building MSI
cmd /c "set VERSION= && msbuild -Restore %SOURCE_ROOT%\wix\Moonlight\Moonlight.wixproj /p:Configuration=%BUILD_CONFIG% /p:Platform=%ARCH% /p:MSBuildProjectExtensionsPath=%BUILD_FOLDER%\"
if !ERRORLEVEL! NEQ 0 goto Error

echo Copying application binary to deployment directory
copy %BUILD_FOLDER%\app\%BUILD_CONFIG%\Moonlight.exe %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error

echo Building portable package
rem This must be done after WiX harvesting and signing, since the VCRT dlls are MS signed
rem and should not be harvested for inclusion in the full installer
copy "%VC_REDIST_DLL_PATH%\*.dll" %DEPLOY_FOLDER%
if !ERRORLEVEL! NEQ 0 goto Error
if /I "%ARCH%"=="arm64" (
    rem The ARM64 MSVC redist can include an x64-only vcruntime140_1.dll. Moonlight
    rem and the bundled ARM64 DLLs do not import it, and shipping it breaks launch.
    del /f /q "%DEPLOY_FOLDER%\vcruntime140_1.dll"
)

rem Portable packages should be portable by default. CI_VERSION is only used to
rem pin artifact versions, so use an explicit opt-out when a CI/debug package
rem should default to the normal per-user settings location.
if defined MOONLIGHT_PORTABLE_INACTIVE (
    echo. > %DEPLOY_FOLDER%\portable.dat.inactive
    if !ERRORLEVEL! NEQ 0 goto Error
) else (
    rem This file tells Moonlight that it's a portable installation
    echo. > %DEPLOY_FOLDER%\portable.dat
    if !ERRORLEVEL! NEQ 0 goto Error
)

7z a %INSTALLER_FOLDER%\MoonlightPortable-%ARCH%-%VERSION%.zip %DEPLOY_FOLDER%\*
if !ERRORLEVEL! NEQ 0 goto Error

echo Build successful for Moonlight v%VERSION% %ARCH% binaries!
exit /b 0

:Error
echo Build failed!
exit /b !ERRORLEVEL!
