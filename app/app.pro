#-------------------------------------------------
#
# Project created by QtCreator 2018-04-28T14:01:01
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = moonlight-qt
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

macx {
    INCLUDEPATH += $$PWD/../libs/mac/include
    LIBS += $$PWD/../libs/mac/lib/libssl.dylib
    LIBS += $$PWD/../libs/mac/lib/libcrypto.dylib
}
unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl
}

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    nvhttp.cpp \
    nvpairingmanager.cpp \
    identitymanager.cpp \
    popupmanager.cpp

HEADERS += \
    mainwindow.h \
    nvhttp.h \
    nvpairingmanager.h \
    identitymanager.h \
    utils.h \
    popupmanager.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    resources.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../moonlight-common-c/release/ -lmoonlight-common-c
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../moonlight-common-c/debug/ -lmoonlight-common-c
else:unix: LIBS += -L$$OUT_PWD/../moonlight-common-c/ -lmoonlight-common-c

INCLUDEPATH += $$PWD/../moonlight-common-c/moonlight-common-c/src
DEPENDPATH += $$PWD/../moonlight-common-c/moonlight-common-c/src

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../opus/release/ -lopus
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../opus/debug/ -lopus
else:unix: LIBS += -L$$OUT_PWD/../opus/ -lopus

INCLUDEPATH += $$PWD/../opus/opus/include
DEPENDPATH += $$PWD/../opus/opus/include
