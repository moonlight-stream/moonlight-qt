TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
QT += core
QT -= gui
INCLUDEPATH += ../../../app ../../../app/streaming/video ../../../libs/mac/include ../../../libs/mac/include/SDL2
SOURCES += overlay-test.cpp ../../../app/streaming/video/overlaymanager.cpp
LIBS += -L$$PWD/../../../libs/mac/lib -lSDL2 -lSDL2_ttf
