QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle

INCLUDEPATH += ../../app

SOURCES += \
    main.cpp \
    ../../app/settings/devicelocalsettings.cpp \
    ../../app/streaming/video/overlaybuttonposition.cpp

HEADERS += \
    ../../app/settings/devicelocalsettings.h \
    ../../app/streaming/video/overlaybuttonposition.h
