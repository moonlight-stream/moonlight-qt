# Microphone capture module for Moonlight-qt

HEADERS += \
    $$PWD/microphonecapture.h \
    $$PWD/sdlmicrophonecapture.h

SOURCES += \
    $$PWD/microphonecapture.cpp \
    $$PWD/sdlmicrophonecapture.cpp

# Opus codec for microphone encoding
CONFIG += link_pkgconfig
packagesExist(opus) {
    PKGCONFIG += opus
    DEFINES += HAVE_OPUS
} else {
    warning("Opus codec not found - microphone capture will be disabled")
}

# Enable microphone logging category
DEFINES += QT_MESSAGELOGCONTEXT