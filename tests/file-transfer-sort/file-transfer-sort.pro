QT += core gui network widgets testlib
CONFIG += c++17 console testcase
TEMPLATE = app
TARGET = file-transfer-sort-test
INCLUDEPATH += $$PWD/../../app $$PWD/../../libs/windows/include $$PWD/../../libs/windows/include/x64
LIBS += -L$$PWD/../../libs/windows/lib/x64 -llibcrypto -luser32 -lshell32 -lole32 -loleaut32 -luuid
SOURCES += $$PWD/main.cpp \
           $$PWD/../../app/streaming/filetransferwindow.cpp \
           $$PWD/../../app/streaming/filetransferengine.cpp \
           $$PWD/../../app/backend/identitymanager.cpp
HEADERS += $$PWD/../../app/streaming/filetransferwindow.h \
           $$PWD/../../app/streaming/filetransferengine.h
