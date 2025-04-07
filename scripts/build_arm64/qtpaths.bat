@echo off
%QT_ROOT_DIR%\..\msvc2022_64\bin\qtpaths.exe -qtconf "%~dp0\target_qt.conf" %*
