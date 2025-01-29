@echo off
%QTDIR%\msvc2022_64\bin\qtpaths.exe -qtconf "%~dp0\target_qt.conf" %*
