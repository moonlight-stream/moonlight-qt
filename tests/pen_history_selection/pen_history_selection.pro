QT -= core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = pen_history_selection_test

INCLUDEPATH += ../../app

SOURCES += main.cpp

HEADERS += ../../app/streaming/input/penhistory.h
