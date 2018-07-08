QT += core quick network
CONFIG += c++11

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

win32 {
    INCLUDEPATH += $$PWD/../libs/windows/include

    contains(QT_ARCH, i386) {
        LIBS += -L$$PWD/../libs/windows/lib/x86
    }
    contains(QT_ARCH, x86_64) {
        LIBS += -L$$PWD/../libs/windows/lib/x64
    }

    LIBS += ws2_32.lib winmm.lib
}
macx {
    INCLUDEPATH += $$PWD/../libs/mac/include
    LIBS += -L$$PWD/../libs/mac/lib
}

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl sdl2 libavcodec libavdevice libavformat libavutil
}
win32 {
    LIBS += -llibssl -llibcrypto -lSDL2 -lavcodec -lavdevice -lavformat -lavutil
}
macx {
    LIBS += -lssl -lcrypto -lSDL2 -lavcodec.58 -lavdevice.58 -lavformat.58 -lavutil.56
}

SOURCES += \
    main.cpp \
    backend/identitymanager.cpp \
    backend/nvhttp.cpp \
    backend/nvpairingmanager.cpp \
    backend/computermanager.cpp \
    backend/boxartmanager.cpp \
    settings/streamingpreferences.cpp \
    streaming/input.cpp \
    streaming/session.cpp \
    streaming/audio.cpp \
    streaming/video.cpp \
    gui/computermodel.cpp \
    gui/appmodel.cpp

HEADERS += \
    utils.h \
    backend/identitymanager.h \
    backend/nvhttp.h \
    backend/nvpairingmanager.h \
    backend/computermanager.h \
    backend/boxartmanager.h \
    settings/streamingpreferences.h \
    streaming/input.hpp \
    streaming/session.hpp \
    gui/computermodel.h \
    gui/appmodel.h

RESOURCES += \
    resources.qrc \
    qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

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

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/release/ -lqmdnsengine
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/debug/ -lqmdnsengine
else:unix: LIBS += -L$$OUT_PWD/../qmdnsengine/ -lqmdnsengine

INCLUDEPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine
DEPENDPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

macx {
    QMAKE_INFO_PLIST = $$PWD/Info.plist
    APP_QML_FILES.files = res/macos.icns
    APP_QML_FILES.path = Contents/Resources
    QMAKE_BUNDLE_DATA += APP_QML_FILES
}
