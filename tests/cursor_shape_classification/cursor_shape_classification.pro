QT += core
CONFIG += c++17 console
CONFIG -= app_bundle

TARGET = cursor_shape_classification
TEMPLATE = app

INCLUDEPATH += \
    $$PWD/../../app

SOURCES += \
    main.cpp \
    ../../app/streaming/input/cursorshapeclassifier.cpp

HEADERS += \
    ../../app/streaming/input/cursorshapeclassifier.h
