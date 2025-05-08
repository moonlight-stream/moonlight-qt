@echo off
%QT_ROOT_DIR%\..\msvc2022_64\bin\qmake.exe -qtconf "%~dp0\target_qt.conf" %*
