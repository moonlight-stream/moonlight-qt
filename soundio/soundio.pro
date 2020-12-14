#-------------------------------------------------
#
# Project created by QtCreator 2018-10-02T15:27:07
#
#-------------------------------------------------

QT       -= core gui

TARGET = soundio
TEMPLATE = lib

# Build a static library
CONFIG += staticlib

# Disable warnings
CONFIG += warn_off

# Include global qmake defs
include(../globaldefs.pri)

# Force MSVC to compile C as C++ for atomic support
*-msvc* {
    QMAKE_CFLAGS += /TP
}

# Older GCC versions defaulted to GNU89
*-g++ {
    QMAKE_CFLAGS += -std=gnu99
}

unix:!macx {
    CONFIG += link_pkgconfig

    packagesExist(libpulse) {
        PKGCONFIG += libpulse
        CONFIG += pulseaudio
    }
    packagesExist(alsa) {
        PKGCONFIG += alsa
        CONFIG += alsa
    }
}

DEFINES += \
    SOUNDIO_STATIC_LIBRARY               \
    SOUNDIO_VERSION_MAJOR=1              \
    SOUNDIO_VERSION_MINOR=1              \
    SOUNDIO_VERSION_PATCH=0              \
    SOUNDIO_VERSION_STRING=\\\"1.1.0\\\"

SRC_DIR = $$PWD/libsoundio/src
INC_DIR = $$PWD/libsoundio

SOURCES += \
    $$SRC_DIR/channel_layout.c \
    $$SRC_DIR/dummy.c          \
    $$SRC_DIR/os.c             \
    $$SRC_DIR/ring_buffer.c    \
    $$SRC_DIR/soundio.c        \
    $$SRC_DIR/util.c

HEADERS += \
    $$SRC_DIR/atomics.h           \
    $$SRC_DIR/dummy.h             \
    $$SRC_DIR/list.h              \
    $$SRC_DIR/os.h                \
    $$SRC_DIR/ring_buffer.h       \
    $$SRC_DIR/soundio_internal.h  \
    $$SRC_DIR/soundio_private.h   \
    $$SRC_DIR/util.h              \
    $$INC_DIR/soundio/soundio.h   \
    $$INC_DIR/soundio/endian.h

INCLUDEPATH += $$INC_DIR

win32 {
    message(WASAPI backend selected)

    DEFINES += SOUNDIO_HAVE_WASAPI
    SOURCES += $$SRC_DIR/wasapi.c
    HEADERS += $$SRC_DIR/wasapi.h
}
macx {
    message(CoreAudio backend selected)

    DEFINES += SOUNDIO_HAVE_COREAUDIO
    SOURCES += $$SRC_DIR/coreaudio.c
    HEADERS += $$SRC_DIR/coreaudio.h
}
pulseaudio {
    message(PulseAudio backend selected)

    DEFINES += SOUNDIO_HAVE_PULSEAUDIO
    SOURCES += $$SRC_DIR/pulseaudio.c
    HEADERS += $$SRC_DIR/pulseaudio.h
}
alsa {
    message(ALSA backend selected)

    DEFINES += SOUNDIO_HAVE_ALSA
    SOURCES += $$SRC_DIR/alsa.c
    HEADERS += $$SRC_DIR/alsa.h
}
