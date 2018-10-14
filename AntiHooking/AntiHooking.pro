QT       -= core gui

TARGET = AntiHooking
TEMPLATE = lib

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Ensure symbols are always generated
CONFIG += force_debug_info

INCLUDEPATH += $$PWD/../libs/windows/include
contains(QT_ARCH, i386) {
    LIBS += -L$$PWD/../libs/windows/lib/x86
}
contains(QT_ARCH, x86_64) {
    LIBS += -L$$PWD/../libs/windows/lib/x64
}

LIBS += -lNktHookLib
DEFINES += ANTIHOOKING_LIBRARY
SOURCES += antihookingprotection.cpp
HEADERS += antihookingprotection.h
