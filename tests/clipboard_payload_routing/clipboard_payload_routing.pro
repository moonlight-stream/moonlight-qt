QT += core gui network
CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = clipboard_payload_routing
TEMPLATE = app

INCLUDEPATH += \
    $$PWD/../../app

SOURCES += \
    main.cpp \
    ../../app/streaming/clipboardsync.cpp

macx:SOURCES += ../../clipboard-helper/macos.mm

HEADERS += \
    ../../app/streaming/clipboardlogging.h \
    ../../app/streaming/clipboardsync.h
