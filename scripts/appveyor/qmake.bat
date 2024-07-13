@echo off
%QTDIR%\msvc2019_64\bin\qmake.exe -qtconf "%~dp0\target_qt.conf" %*
