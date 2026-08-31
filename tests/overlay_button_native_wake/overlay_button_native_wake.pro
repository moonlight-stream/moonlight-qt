QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle

win32: LIBS += user32.lib

INCLUDEPATH += ../../app

SOURCES += \
    ../../app/settings/devicelocalsettings.cpp \
    ../../app/streaming/video/overlaybuttonposition.cpp \
    ../../app/streaming/video/overlaymenubutton.cpp

macx {
    SOURCES += \
        main_mac.mm \
        ../../app/streaming/video/overlayeventmonitor_mac.mm
} else {
    SOURCES += main.cpp
}

HEADERS += \
    ../../app/settings/devicelocalsettings.h \
    ../../app/streaming/video/overlaybuttonposition.h \
    ../../app/streaming/video/overlayeventwakestate.h \
    ../../app/streaming/video/overlaymenubutton.h

macx: HEADERS += ../../app/streaming/video/overlayeventmonitor_mac.h
