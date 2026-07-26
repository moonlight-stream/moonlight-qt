#-------------------------------------------------
#
# Project created by QtCreator 2018-10-12T15:50:59
#
#-------------------------------------------------


QT       -= core gui

TARGET = h264bitstream
TEMPLATE = lib

# Build a static library
CONFIG += staticlib

# Disable warnings
CONFIG += warn_off

# Include global qmake defs
include(../globaldefs.pri)

# Older GCC versions defaulted to GNU89
*-g++ {
    QMAKE_CFLAGS += -std=gnu99
}

SOURCES += \
    h264_nal.c \
    h264_stream.c

HEADERS += \
    bs.h \
    h264_stream.h
