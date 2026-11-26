QT = core testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TARGET = audio-startup-test

APP_DIR = $$PWD/../../app
INCLUDEPATH += $$APP_DIR $$PWD/../../moonlight-common-c/moonlight-common-c/src
win32 {
    SDL_ARCH = $$QT_ARCH
    equals(SDL_ARCH, x86_64): SDL_ARCH = x64
    INCLUDEPATH += $$PWD/../../libs/windows/include/$$SDL_ARCH/SDL2
    LIBS += -L$$PWD/../../libs/windows/lib/$$SDL_ARCH -lSDL2
} else {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2
}
SOURCES += main.cpp $$APP_DIR/backend/localhostsessions.cpp $$APP_DIR/streaming/audio/renderers/sdlaud.cpp
HEADERS += $$APP_DIR/backend/localhostsessions.h
