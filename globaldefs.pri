# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Ensure symbols are always generated
CONFIG += force_debug_info

# Disable asserts on release builds
CONFIG(release, debug|release) {
    DEFINES += NDEBUG
}

# Enable CFG, EHCont, and CET
*-msvc {
    QMAKE_CFLAGS += -guard:cf -guard:ehcont
    QMAKE_CXXFLAGS += -guard:cf -guard:ehcont
    QMAKE_LFLAGS += -guard:cf -guard:ehcont

    contains(QT_ARCH, x86_64) {
        QMAKE_LFLAGS += -cetcompat
    }
}

# Enable ASan for Linux or macOS
#CONFIG += sanitizer sanitize_address

# Enable ASan for Windows
#QMAKE_CFLAGS += -fsanitize=address
#QMAKE_CXXFLAGS += -fsanitize=address
#QMAKE_LFLAGS += -incremental:no -wholearchive:clang_rt.asan_dynamic-x86_64.lib -wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib

# Propagate environment variable flags
QMAKE_CFLAGS   += $$(CFLAGS)
QMAKE_CXXFLAGS += $$(CXXFLAGS)
QMAKE_LFLAGS   += $$(LDFLAGS)

# Qt 6 qyieldcpu.h calls __yield() on AArch64 without including <arm_acle.h>.
# Apple Clang then treats it as an implicit declaration; with -Werror (e.g. from
# CXXFLAGS) the build fails. Force the ACLE header so the intrinsic is declared.
macx:contains(QT_ARCH, arm64) {
    QMAKE_CXXFLAGS += -include arm_acle.h
}
