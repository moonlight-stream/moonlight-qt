TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    opus \
    qmdnsengine \
    app

CONFIG += ordered

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Run our compile tests
load(configure)
qtCompileTest(SLVideo)
