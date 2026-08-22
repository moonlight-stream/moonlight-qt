QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = stylus_replay_test

DEFINES += MOONLIGHT_ENABLE_FUNCTION_TESTS
INCLUDEPATH += ../../app

SOURCES += \
    main.cpp \
    ../../app/streaming/input/stylusreplay.cpp

HEADERS += ../../app/streaming/input/stylusreplay.h
