QT -= core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = ds5_ir_renderer_test

INCLUDEPATH += \
    ../../app \
    ../../moonlight-common-c/moonlight-common-c/src

SOURCES += main.cpp
