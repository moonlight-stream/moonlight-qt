# Privileged helper tool for Moonlight's "Disable AWDL while streaming"
# setting. Built only on macOS (see moonlight-qt.pro).
#
# The two plists are embedded into the binary as Mach-O sections, which is
# how SMJobBless finds them:
#   __TEXT,__info_plist    -> SMAuthorizedClients (which clients may install us)
#   __TEXT,__launchd_plist -> launchd job (Label + MachServices)
# On install, SMJobBless copies this tool to /Library/PrivilegedHelperTools/
# and the launchd plist to /Library/LaunchDaemons/. It is started on demand
# by launchd when the app connects to its Mach service, and exits when idle.
#
# The helper is pure Foundation/Security -- no Qt, no GUI code (GUI code
# must never run as root). It must be signed with the same identity as the
# Moonlight app, or SMJobBless will refuse to install it.

TEMPLATE = app
CONFIG -= app_bundle
QT -= core gui

TARGET = com.moonlight-stream.Moonlight.AWDLHelper

OBJECTIVE_SOURCES += MoonlightAWDLHelper.m
HEADERS += AWDLHelperProtocol.h

LIBS += -framework Foundation -framework Security

QMAKE_LFLAGS += -sectcreate __TEXT __info_plist $$PWD/MoonlightAWDLHelper-Info.plist
QMAKE_LFLAGS += -sectcreate __TEXT __launchd_plist $$PWD/MoonlightAWDLHelper-Launchd.plist
