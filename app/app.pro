QT += core quick network quickcontrols2 svg
CONFIG += c++11

unix:!macx {
    TARGET = moonlight
} else {
    # On macOS, this is the name displayed in the global menu bar
    TARGET = Moonlight
}

include(../globaldefs.pri)

# Precompile QML files to avoid writing qmlcache on portable versions.
# Since this binds the app against the Qt runtime version, we will only
# do this for Windows and Mac, since they ship with the Qt runtime.
win32|macx {
    CONFIG(release, debug|release) {
        CONFIG += qtquickcompiler
    }
}

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
    INCLUDEPATH += \
        $$PWD/../libs/windows/include \
        $$(DXSDK_DIR)/Include

    contains(QT_ARCH, i386) {
        LIBS += -L$$PWD/../libs/windows/lib/x86
        LIBS += -L$$(DXSDK_DIR)/Lib/x86
    }
    contains(QT_ARCH, x86_64) {
        LIBS += -L$$PWD/../libs/windows/lib/x64
        LIBS += -L$$(DXSDK_DIR)/Lib/x64
    }

    LIBS += ws2_32.lib winmm.lib dxva2.lib ole32.lib gdi32.lib user32.lib d3d9.lib dwmapi.lib dbghelp.lib
}
macx {
    INCLUDEPATH += $$PWD/../libs/mac/include
    INCLUDEPATH += $$PWD/../libs/mac/Frameworks/SDL2.framework/Versions/A/Headers
    INCLUDEPATH += $$PWD/../libs/mac/Frameworks/SDL2_ttf.framework/Versions/A/Headers
    LIBS += -L$$PWD/../libs/mac/lib -F$$PWD/../libs/mac/Frameworks
}

unix:!macx {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl sdl2 SDL2_ttf opus

    # SLAudio is used on Steam Link
    !config_SL {
        CONFIG += soundio
    }

    soundio {
        packagesExist(libpulse) {
            PKGCONFIG += libpulse
        }
        packagesExist(alsa) {
            PKGCONFIG += alsa
        }
    }

    packagesExist(libavcodec) {
        PKGCONFIG += libavcodec libavutil
        CONFIG += ffmpeg

        packagesExist(libva) {
            packagesExist(libva-x11) {
                CONFIG += libva-x11
            }
            packagesExist(libva-wayland) {
                CONFIG += libva-wayland
            }
            CONFIG += libva
        }

        packagesExist(vdpau) {
            CONFIG += libvdpau
        }

        packagesExist(mmal) {
            PKGCONFIG += mmal
            CONFIG += mmal
        }

        packagesExist(libdrm) {
            PKGCONFIG += libdrm
            CONFIG += libdrm
        }
    }

    packagesExist(jsoncpp sigc++-2.0) {
        DEFINES += HAVE_I3IPC

        INCLUDEPATH += $$PWD/../libs/unix/include
        SOURCES += \
            $$PWD/../libs/unix/src/i3ipc++/ipc.cpp \
            $$PWD/../libs/unix/src/i3ipc++/ipc-util.cpp

        PKGCONFIG += sigc++-2.0 jsoncpp
    }
}
win32 {
    LIBS += -llibssl -llibcrypto -lSDL2 -lSDL2_ttf -lavcodec -lavutil -lopus -ld3dx9
    CONFIG += ffmpeg
}
win32:!winrt {
    CONFIG += soundio
}
macx {
    LIBS += -lssl -lcrypto -lavcodec.58 -lavutil.56 -lopus -framework SDL2 -framework SDL2_ttf
    LIBS += -lobjc -framework VideoToolbox -framework AVFoundation -framework CoreVideo -framework CoreGraphics -framework CoreMedia -framework AppKit

    # For libsoundio
    LIBS += -framework CoreAudio -framework AudioUnit

    CONFIG += ffmpeg soundio
}

SOURCES += \
    main.cpp \
    backend/computerseeker.cpp \
    backend/identitymanager.cpp \
    backend/nvcomputer.cpp \
    backend/nvhttp.cpp \
    backend/nvpairingmanager.cpp \
    backend/computermanager.cpp \
    backend/boxartmanager.cpp \
    cli/commandlineparser.cpp \
    cli/quitstream.cpp \
    cli/startstream.cpp \
    settings/streamingpreferences.cpp \
    streaming/input.cpp \
    streaming/session.cpp \
    streaming/audio/audio.cpp \
    streaming/audio/renderers/sdlaud.cpp \
    gui/computermodel.cpp \
    gui/appmodel.cpp \
    streaming/streamutils.cpp \
    backend/autoupdatechecker.cpp \
    path.cpp \
    settings/mappingmanager.cpp \
    gui/sdlgamepadkeynavigation.cpp \
    streaming/video/overlaymanager.cpp \
    backend/systemproperties.cpp

HEADERS += \
    utils.h \
    backend/computerseeker.h \
    backend/identitymanager.h \
    backend/nvcomputer.h \
    backend/nvhttp.h \
    backend/nvpairingmanager.h \
    backend/computermanager.h \
    backend/boxartmanager.h \
    cli/commandlineparser.h \
    cli/quitstream.h \
    cli/startstream.h \
    settings/streamingpreferences.h \
    streaming/input.h \
    streaming/session.h \
    streaming/audio/renderers/renderer.h \
    streaming/audio/renderers/sdl.h \
    gui/computermodel.h \
    gui/appmodel.h \
    streaming/video/decoder.h \
    streaming/streamutils.h \
    backend/autoupdatechecker.h \
    path.h \
    settings/mappingmanager.h \
    gui/sdlgamepadkeynavigation.h \
    streaming/video/overlaymanager.h \
    backend/systemproperties.h

# Platform-specific renderers and decoders
ffmpeg {
    message(FFmpeg decoder selected)

    DEFINES += HAVE_FFMPEG
    SOURCES += \
        streaming/video/ffmpeg.cpp \
        streaming/video/ffmpeg-renderers/sdlvid.cpp \
        streaming/video/ffmpeg-renderers/pacer/pacer.cpp \
        streaming/video/ffmpeg-renderers/pacer/nullthreadedvsyncsource.cpp

    HEADERS += \
        streaming/video/ffmpeg.h \
        streaming/video/ffmpeg-renderers/renderer.h \
        streaming/video/ffmpeg-renderers/sdlvid.h \
        streaming/video/ffmpeg-renderers/pacer/pacer.h \
        streaming/video/ffmpeg-renderers/pacer/nullthreadedvsyncsource.h
}
libva {
    message(VAAPI renderer selected)

    PKGCONFIG += libva
    DEFINES += HAVE_LIBVA
    SOURCES += streaming/video/ffmpeg-renderers/vaapi.cpp
    HEADERS += streaming/video/ffmpeg-renderers/vaapi.h
}
libva-x11 {
    message(VAAPI X11 support enabled)

    PKGCONFIG += libva-x11
    DEFINES += HAVE_LIBVA_X11
}
libva-wayland {
    message(VAAPI Wayland support enabled)

    PKGCONFIG += libva-wayland
    DEFINES += HAVE_LIBVA_WAYLAND
}
libvdpau {
    message(VDPAU renderer selected)

    DEFINES += HAVE_LIBVDPAU
    SOURCES += streaming/video/ffmpeg-renderers/vdpau.cpp
    HEADERS += streaming/video/ffmpeg-renderers/vdpau.h
}
mmal {
    message(MMAL renderer selected)

    DEFINES += HAVE_MMAL
    SOURCES += streaming/video/ffmpeg-renderers/mmal.cpp
    HEADERS += streaming/video/ffmpeg-renderers/mmal.h
}
libdrm {
    message(DRM renderer selected)

    DEFINES += HAVE_DRM
    SOURCES += streaming/video/ffmpeg-renderers/drm.cpp
    HEADERS += streaming/video/ffmpeg-renderers/drm.h
}
config_SL {
    message(Steam Link build configuration selected)

    DEFINES += STEAM_LINK HAVE_SLVIDEO HAVE_SLAUDIO
    LIBS += -lSLVideo -lSLAudio

    SOURCES += \
        streaming/video/slvid.cpp \
        streaming/audio/renderers/slaud.cpp
    HEADERS += \
        streaming/video/slvid.h \
        streaming/audio/renderers/slaud.h
}
win32:!winrt {
    message(DXVA2 renderer selected)

    SOURCES += \
        streaming/video/ffmpeg-renderers/dxva2.cpp \
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.cpp

    HEADERS += \
        streaming/video/ffmpeg-renderers/dxva2.h \
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.h
}
macx {
    message(VideoToolbox renderer selected)

    SOURCES += \
        streaming/video/ffmpeg-renderers/vt.mm \
        streaming/video/ffmpeg-renderers/pacer/displaylinkvsyncsource.mm

    HEADERS += \
        streaming/video/ffmpeg-renderers/vt.h \
        streaming/video/ffmpeg-renderers/pacer/displaylinkvsyncsource.h
}
soundio {
    message(libsoundio audio renderer selected)

    DEFINES += HAVE_SOUNDIO SOUNDIO_STATIC_LIBRARY
    SOURCES += streaming/audio/renderers/soundioaudiorenderer.cpp
    HEADERS += streaming/audio/renderers/soundioaudiorenderer.h
}

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

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/release/ -lqmdnsengine
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../qmdnsengine/debug/ -lqmdnsengine
else:unix: LIBS += -L$$OUT_PWD/../qmdnsengine/ -lqmdnsengine

INCLUDEPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine
DEPENDPATH += $$PWD/../qmdnsengine/qmdnsengine/src/include $$PWD/../qmdnsengine

soundio {
    win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../soundio/release/ -lsoundio
    else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../soundio/debug/ -lsoundio
    else:unix: LIBS += -L$$OUT_PWD/../soundio/ -lsoundio

    INCLUDEPATH += $$PWD/../soundio/libsoundio
    DEPENDPATH += $$PWD/../soundio/libsoundio
}

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../h264bitstream/release/ -lh264bitstream
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../h264bitstream/debug/ -lh264bitstream
else:unix: LIBS += -L$$OUT_PWD/../h264bitstream/ -lh264bitstream

INCLUDEPATH += $$PWD/../h264bitstream/h264bitstream
DEPENDPATH += $$PWD/../h264bitstream/h264bitstream

!winrt {
    win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../AntiHooking/release/ -lAntiHooking
    else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../AntiHooking/debug/ -lAntiHooking

    INCLUDEPATH += $$PWD/../AntiHooking
    DEPENDPATH += $$PWD/../AntiHooking
}

unix:!macx: {
    isEmpty(PREFIX) {
        PREFIX = /usr/local
    }
    isEmpty(BINDIR) {
        BINDIR = bin
    }
    isEmpty(DATADIR) {
        DATADIR = share
    }

    target.path = $$PREFIX/$$BINDIR/

    desktop.files = deploy/linux/com.moonlight_stream.Moonlight.desktop
    desktop.path = $$PREFIX/$$DATADIR/applications/

    icons.files = res/moonlight.svg
    icons.path = $$PREFIX/$$DATADIR/icons/hicolor/scalable/apps/

    appstream.files = deploy/linux/com.moonlight_stream.Moonlight.appdata.xml
    appstream.path = $$PREFIX/$$DATADIR/metainfo/

    INSTALLS += target desktop icons appstream
}
win32 {
    RC_ICONS = moonlight.ico
    QMAKE_TARGET_COMPANY = Moonlight Game Streaming Project
    QMAKE_TARGET_DESCRIPTION = Moonlight Game Streaming Client
    QMAKE_TARGET_PRODUCT = Moonlight

    CONFIG -= embed_manifest_exe
    QMAKE_LFLAGS += /MANIFEST:embed /MANIFESTINPUT:$${PWD}/Moonlight.exe.manifest
}
macx {
    # Create Info.plist in object dir with the correct version string
    system(cp $$PWD/Info.plist $$OUT_PWD/Info.plist)
    system(sed -i -e 's/VERSION/$$cat(version.txt)/g' $$OUT_PWD/Info.plist)

    QMAKE_INFO_PLIST = $$OUT_PWD/Info.plist

    APP_BUNDLE_RESOURCES.files = moonlight.icns
    APP_BUNDLE_RESOURCES.path = Contents/Resources

    APP_BUNDLE_FRAMEWORKS.files = $$files(../libs/mac/Frameworks/*.framework, true)
    APP_BUNDLE_FRAMEWORKS.path = Contents/Frameworks

    QMAKE_BUNDLE_DATA += APP_BUNDLE_RESOURCES APP_BUNDLE_FRAMEWORKS

    QMAKE_RPATHDIR += @executable_path/../Frameworks
}

VERSION = "$$cat(version.txt)"
DEFINES += VERSION_STR=\\\"$$cat(version.txt)\\\"
