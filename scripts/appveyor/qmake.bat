@echo off
%QTDIR_ARM64%\msvc2019_64\bin\qmake.exe -qtconf "%~dp0\target_qt.conf" %*
