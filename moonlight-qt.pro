TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    qmdnsengine \
    app \
    soundio

# Build the dependencies in parallel before the final app
app.depends = qmdnsengine moonlight-common-c soundio

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Run our compile tests
load(configure)
qtCompileTest(SLVideo)
