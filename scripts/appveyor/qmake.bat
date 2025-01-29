@echo off
%QTDIR%\msvc2022_64\bin\qmake.exe -qtconf "%~dp0\target_qt.conf" %*
