TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    qmdnsengine \
    app \
    h264bitstream

# Build the dependencies in parallel before the final app
app.depends = qmdnsengine moonlight-common-c h264bitstream
win32:!winrt {
    contains(QT_ARCH, i386)|contains(QT_ARCH, x86_64) {
        # We don't build AntiHooking.dll for ARM64 (yet?)
        SUBDIRS += AntiHooking
        app.depends += AntiHooking
    }
}
!winrt:win32|macx {
    SUBDIRS += soundio
    app.depends += soundio
}

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Run our compile tests
load(configure)
qtCompileTest(SL)
qtCompileTest(EGL)
