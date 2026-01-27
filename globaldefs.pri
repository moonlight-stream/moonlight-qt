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

# Fix for macOS Command Line Tools - add C++ headers from SDK to include path
macx {
    SDK_PATH = $$system(xcrun --show-sdk-path 2>/dev/null || echo "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")
    isEmpty(SDK_PATH) {
        SDK_PATH = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
    }

    CXX_HEADERS_PATH = $$SDK_PATH/usr/include/c++/v1
    exists($$CXX_HEADERS_PATH) {
        QMAKE_CXXFLAGS += -I$$CXX_HEADERS_PATH
        QMAKE_CFLAGS += -I$$CXX_HEADERS_PATH
    }
}
