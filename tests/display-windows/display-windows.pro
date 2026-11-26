QT = core
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = display-windows-test
INCLUDEPATH += ../../app/streaming
SOURCES += main.cpp ../../app/streaming/streamwindow.cpp
LIBS += -luser32 -ldwmapi
