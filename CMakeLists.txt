cmake_minimum_required(VERSION 3.21)
project(moonlight-qt LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zi /guard:cf /guard:ehcont)
    add_link_options(/DEBUG /guard:cf /guard:ehcont)
    if(MSVC_CXX_ARCHITECTURE_ID MATCHES "^x64$")
        add_link_options(/CETCOMPAT)
    endif()
else()
    add_compile_options(-g)
endif()

if(NOT DEFINED CACHE{BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
endif()

if(WIN32)
    enable_language(RC)

    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/libs/windows)
        message(FATAL_ERROR "Missing dependencies. Please run 'powershell .\\setup-deps.ps1' to fetch prebuilt libraries.")
    endif()

    if(MSVC_CXX_ARCHITECTURE_ID MATCHES "ARM64")
        set(MOONLIGHT_WIN_ARCH arm64)
    else()
        set(MOONLIGHT_WIN_ARCH x64)
    endif()

    set(OPENSSL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libs/windows/include CACHE PATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_SOURCE_DIR}/libs/windows/lib/${MOONLIGHT_WIN_ARCH}/libcrypto.lib CACHE FILEPATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY ${CMAKE_SOURCE_DIR}/libs/windows/lib/${MOONLIGHT_WIN_ARCH}/libssl.lib CACHE FILEPATH "" FORCE)
elseif(APPLE)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/libs/mac)
        message(FATAL_ERROR "Missing dependencies. Please run 'python3 setup-deps.py' to fetch prebuilt libraries.")
    endif()

    set(OPENSSL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/libs/mac/include CACHE PATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_SOURCE_DIR}/libs/mac/lib/libcrypto.3.dylib CACHE FILEPATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY ${CMAKE_SOURCE_DIR}/libs/mac/lib/libssl.3.dylib CACHE FILEPATH "" FORCE)
endif()

add_subdirectory(h264bitstream)
add_subdirectory(moonlight-common-c/moonlight-common-c)
add_subdirectory(qmdnsengine/qmdnsengine)

if(WIN32)
    add_subdirectory(AntiHooking)
endif()

add_subdirectory(app)
