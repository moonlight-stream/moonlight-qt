QT += core network gui
CONFIG += c++17 console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = quit-app-test
INCLUDEPATH += $$PWD/../../app $$PWD/../../moonlight-common-c/moonlight-common-c/src
win32 {
    INCLUDEPATH += $$PWD/../../libs/windows/include $$PWD/../../libs/windows/include/x64
    LIBS += -L$$PWD/../../libs/windows/lib/x64 -llibcrypto
} else {
    LIBS += -lcrypto
}
SOURCES += $$PWD/main.cpp \
    $$PWD/../../app/backend/nvhttp.cpp \
    $$PWD/../../app/backend/nvaddress.cpp \
    $$PWD/../../app/backend/nvapp.cpp \
    $$PWD/../../app/backend/identitymanager.cpp
HEADERS += $$PWD/../../app/backend/nvhttp.h
