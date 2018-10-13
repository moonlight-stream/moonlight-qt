#-------------------------------------------------
#
# Project created by QtCreator 2018-10-12T15:50:59
#
#-------------------------------------------------


QT       -= core gui

TARGET = h264bitstream
TEMPLATE = lib

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Ensure symbols are always generated
CONFIG += force_debug_info

# Build a static library
CONFIG += staticlib

# Disable warnings
CONFIG += warn_off

SRC_DIR = $$PWD/h264bitstream

SOURCES += \
    $$SRC_DIR/h264_avcc.c           \
    $$SRC_DIR/h264_nal.c            \
    $$SRC_DIR/h264_sei.c            \
    $$SRC_DIR/h264_slice_data.c     \
    $$SRC_DIR/h264_stream.c

HEADERS += \
    $$SRC_DIR/bs.h              \
    $$SRC_DIR/h264_avcc.h       \
    $$SRC_DIR/h264_sei.h        \
    $$SRC_DIR/h264_slice_data.h \
    $$SRC_DIR/h264_stream.h

INCLUDEPATH += $$INC_DIR
