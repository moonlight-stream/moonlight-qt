TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    opus \
    qmdnsengine \
    app

CONFIG += ordered

# Run our compile tests
load(configure)
qtCompileTest(SLVideo)
