QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle

win32: LIBS += user32.lib

INCLUDEPATH += ../../app

SOURCES += \
    main.cpp \
    ../../app/settings/devicelocalsettings.cpp \
    ../../app/streaming/video/overlaybuttonposition.cpp \
    ../../app/streaming/video/overlaymenubutton.cpp

HEADERS += \
    ../../app/settings/devicelocalsettings.h \
    ../../app/streaming/video/overlaybuttonposition.h \
    ../../app/streaming/video/overlayeventwakestate.h \
    ../../app/streaming/video/overlaymenubutton.h
