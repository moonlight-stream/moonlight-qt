find_package(Qt6 REQUIRED COMPONENTS Core Quick Network QuickControls2 Svg)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(UNIX AND NOT APPLE)
    set(APP_TARGET moonlight)
else()
    set(APP_TARGET Moonlight)
endif()

option(DISABLE_PREBUILTS "Do not use prebuilt dependency binaries" OFF)
option(DISABLE_FFMPEG "Disable the FFmpeg-based decoder" OFF)
option(DISABLE_LIBVA "Disable VAAPI support" OFF)
option(DISABLE_X11 "Disable X11 support" OFF)
option(DISABLE_WAYLAND "Disable Wayland support" OFF)
option(DISABLE_LIBDRM "Disable DRM support" OFF)
option(DISABLE_LIBVDPAU "Disable VDPAU support" OFF)
option(DISABLE_MMAL "Disable MMAL support" OFF)
option(ENABLE_CUDA "Enable CUDA interop support" OFF)
option(DISABLE_LIBPLACEBO "Disable libplacebo (Vulkan) support" OFF)
option(DISABLE_MASTERHOOKS "Disable DRM master hooks" OFF)
option(ALLOW_EGL_WITH_MMAL "Allow the EGL renderer alongside MMAL" OFF)
option(EMBEDDED "Embedded build" OFF)
option(GLSLOW "Assume a slow GL implementation" OFF)
option(VKSLOW "Assume a slow Vulkan implementation" OFF)
option(GPUSLOW "Assume a slow GL and Vulkan implementation" OFF)

add_executable(${APP_TARGET}
    backend/nvaddress.cpp
    backend/nvapp.cpp
    cli/pair.cpp
    main.cpp
    backend/computerseeker.cpp
    backend/identitymanager.cpp
    backend/nvcomputer.cpp
    backend/nvhttp.cpp
    backend/nvpairingmanager.cpp
    backend/computermanager.cpp
    backend/boxartmanager.cpp
    backend/richpresencemanager.cpp
    cli/commandlineparser.cpp
    cli/listapps.cpp
    cli/quitstream.cpp
    cli/startstream.cpp
    settings/compatfetcher.cpp
    settings/mappingfetcher.cpp
    settings/streamingpreferences.cpp
    streaming/input/abstouch.cpp
    streaming/input/gamepad.cpp
    streaming/input/input.cpp
    streaming/input/keyboard.cpp
    streaming/input/mouse.cpp
    streaming/input/reltouch.cpp
    streaming/session.cpp
    streaming/audio/audio.cpp
    streaming/audio/renderers/sdlaud.cpp
    gui/computermodel.cpp
    gui/appmodel.cpp
    streaming/bandwidth.cpp
    streaming/streamutils.cpp
    backend/autoupdatechecker.cpp
    path.cpp
    settings/mappingmanager.cpp
    gui/sdlgamepadkeynavigation.cpp
    streaming/video/overlaymanager.cpp
    backend/systemproperties.cpp
    wm.cpp

    SDL_compat.h
    backend/nvaddress.h
    backend/nvapp.h
    cli/pair.h
    settings/compatfetcher.h
    settings/mappingfetcher.h
    utils.h
    backend/computerseeker.h
    backend/identitymanager.h
    backend/nvcomputer.h
    backend/nvhttp.h
    backend/nvpairingmanager.h
    backend/computermanager.h
    backend/boxartmanager.h
    backend/richpresencemanager.h
    cli/commandlineparser.h
    cli/listapps.h
    cli/quitstream.h
    cli/startstream.h
    settings/streamingpreferences.h
    streaming/input/input.h
    streaming/session.h
    streaming/audio/renderers/renderer.h
    streaming/audio/renderers/sdl.h
    gui/computermodel.h
    gui/appmodel.h
    streaming/video/decoder.h
    streaming/bandwidth.h
    streaming/streamutils.h
    backend/autoupdatechecker.h
    path.h
    settings/mappingmanager.h
    gui/sdlgamepadkeynavigation.h
    streaming/video/overlaymanager.h
    backend/systemproperties.h
)

target_include_directories(${APP_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(${APP_TARGET} PRIVATE
    QT_DEPRECATED_WARNINGS
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/version.txt MOONLIGHT_VERSION)
target_compile_definitions(${APP_TARGET} PRIVATE VERSION_STR="${MOONLIGHT_VERSION}")

qt6_add_resources(APP_QRC_SOURCES resources.qrc qml.qrc)
target_sources(${APP_TARGET} PRIVATE ${APP_QRC_SOURCES})

target_link_libraries(${APP_TARGET} PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::QuickControls2
    Qt6::Svg
    OpenSSL::SSL
    OpenSSL::Crypto
    moonlight-common-c
    qmdnsengine
    h264bitstream
)

if(WIN32)
    target_include_directories(${APP_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/windows/include
        ${CMAKE_SOURCE_DIR}/libs/windows/include/${MOONLIGHT_WIN_ARCH}
        ${CMAKE_SOURCE_DIR}/libs/windows/include/${MOONLIGHT_WIN_ARCH}/SDL2
        ${CMAKE_SOURCE_DIR}/AntiHooking
    )
    target_link_directories(${APP_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/libs/windows/lib/${MOONLIGHT_WIN_ARCH})
    target_link_libraries(${APP_TARGET} PRIVATE
        ws2_32 winmm dxva2 ole32 gdi32 user32 d3d9 dwmapi dbghelp
        SDL2 SDL2_ttf avcodec avutil swscale opus dxgi d3d11 libplacebo
        discord-rpc AntiHooking
    )
    target_compile_definitions(${APP_TARGET} PRIVATE HAVE_FFMPEG HAVE_LIBPLACEBO_VULKAN HAVE_DISCORD)
    target_sources(${APP_TARGET} PRIVATE
        streaming/video/ffmpeg.cpp
        streaming/video/ffmpeg-renderers/genhwaccel.cpp
        streaming/video/ffmpeg-renderers/sdlvid.cpp
        streaming/video/ffmpeg-renderers/swframemapper.cpp
        streaming/video/ffmpeg-renderers/pacer/pacer.cpp
        streaming/video/ffmpeg-renderers/plvk.cpp
        streaming/video/ffmpeg-renderers/plvk_c.c
        streaming/video/ffmpeg-renderers/dxva2.cpp
        streaming/video/ffmpeg-renderers/d3d11va.cpp
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.cpp
        streaming/video/ffmpeg.h
        streaming/video/ffmpeg-renderers/renderer.h
        streaming/video/ffmpeg-renderers/genhwaccel.h
        streaming/video/ffmpeg-renderers/sdlvid.h
        streaming/video/ffmpeg-renderers/swframemapper.h
        streaming/video/ffmpeg-renderers/pacer/pacer.h
        streaming/video/ffmpeg-renderers/plvk.h
        streaming/video/ffmpeg-renderers/dxutil.h
        streaming/video/ffmpeg-renderers/dxva2.h
        streaming/video/ffmpeg-renderers/d3d11va.h
        streaming/video/ffmpeg-renderers/pacer/dxvsyncsource.h
        moonlight.rc
    )

    if(MSVC)
        target_link_options(${APP_TARGET} PRIVATE /MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/Moonlight.exe.manifest)
    endif()
endif()

if(APPLE)
    if(NOT DISABLE_PREBUILTS)
        target_include_directories(${APP_TARGET} PRIVATE
            ${CMAKE_SOURCE_DIR}/libs/mac/include
            ${CMAKE_SOURCE_DIR}/libs/mac/include/SDL2
        )
        target_link_directories(${APP_TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/libs/mac/lib)
        target_link_libraries(${APP_TARGET} PRIVATE
            ssl.3 crypto.3 avcodec.63 avutil.61 swscale.10 opus.0 SDL2 SDL2_ttf placebo discord-rpc
        )
        target_compile_definitions(${APP_TARGET} PRIVATE HAVE_DISCORD HAVE_LIBPLACEBO_VULKAN)
        target_sources(${APP_TARGET} PRIVATE
            streaming/video/ffmpeg-renderers/plvk.cpp
            streaming/video/ffmpeg-renderers/plvk_c.c
            streaming/video/ffmpeg-renderers/plvk_objc.mm
            streaming/video/ffmpeg-renderers/plvk.h
        )

        file(GLOB MOONLIGHT_MAC_FRAMEWORKS ${CMAKE_SOURCE_DIR}/libs/mac/Frameworks/*.framework)
        file(GLOB MOONLIGHT_MAC_DYLIBS ${CMAKE_SOURCE_DIR}/libs/mac/lib/*.dylib)
        set_source_files_properties(${MOONLIGHT_MAC_FRAMEWORKS} ${MOONLIGHT_MAC_DYLIBS}
            PROPERTIES MACOSX_PACKAGE_LOCATION Contents/Frameworks
        )
        target_sources(${APP_TARGET} PRIVATE ${MOONLIGHT_MAC_FRAMEWORKS} ${MOONLIGHT_MAC_DYLIBS})
        set_target_properties(${APP_TARGET} PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks"
        )
    endif()

    target_link_libraries(${APP_TARGET} PRIVATE
        objc
        "-framework VideoToolbox" "-framework AVFoundation" "-framework CoreVideo"
        "-framework CoreGraphics" "-framework CoreMedia" "-framework AppKit"
        "-framework Metal" "-framework QuartzCore"
    )
    target_compile_definitions(${APP_TARGET} PRIVATE HAVE_FFMPEG)
    target_sources(${APP_TARGET} PRIVATE
        streaming/video/ffmpeg.cpp
        streaming/video/ffmpeg-renderers/genhwaccel.cpp
        streaming/video/ffmpeg-renderers/sdlvid.cpp
        streaming/video/ffmpeg-renderers/swframemapper.cpp
        streaming/video/ffmpeg-renderers/pacer/pacer.cpp
        streaming/video/ffmpeg-renderers/vt_base.mm
        streaming/video/ffmpeg-renderers/vt_avsamplelayer.mm
        streaming/video/ffmpeg-renderers/vt_metal.mm
        streaming/video/ffmpeg.h
        streaming/video/ffmpeg-renderers/renderer.h
        streaming/video/ffmpeg-renderers/genhwaccel.h
        streaming/video/ffmpeg-renderers/sdlvid.h
        streaming/video/ffmpeg-renderers/swframemapper.h
        streaming/video/ffmpeg-renderers/pacer/pacer.h
        streaming/video/ffmpeg-renderers/vt.h
    )

    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist MOONLIGHT_INFO_PLIST)
    string(REPLACE "VERSION" "${MOONLIGHT_VERSION}" MOONLIGHT_INFO_PLIST "${MOONLIGHT_INFO_PLIST}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/Info.plist "${MOONLIGHT_INFO_PLIST}")

    set_source_files_properties(moonlight.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${APP_TARGET} PRIVATE moonlight.icns)
    set_target_properties(${APP_TARGET} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        MACOSX_BUNDLE_ICON_FILE moonlight.icns
    )
endif()

if(UNIX AND NOT APPLE)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
    pkg_check_modules(OPUS REQUIRED IMPORTED_TARGET opus)
    target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_TTF PkgConfig::OPUS)

    if(NOT DISABLE_FFMPEG)
        pkg_check_modules(FFMPEG IMPORTED_TARGET libavcodec libavutil libswscale)
    endif()

    if(FFMPEG_FOUND)
        target_compile_definitions(${APP_TARGET} PRIVATE HAVE_FFMPEG)
        target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::FFMPEG)
        target_sources(${APP_TARGET} PRIVATE
            streaming/video/ffmpeg.cpp
            streaming/video/ffmpeg-renderers/genhwaccel.cpp
            streaming/video/ffmpeg-renderers/sdlvid.cpp
            streaming/video/ffmpeg-renderers/swframemapper.cpp
            streaming/video/ffmpeg-renderers/pacer/pacer.cpp
            streaming/video/ffmpeg.h
            streaming/video/ffmpeg-renderers/renderer.h
            streaming/video/ffmpeg-renderers/genhwaccel.h
            streaming/video/ffmpeg-renderers/sdlvid.h
            streaming/video/ffmpeg-renderers/swframemapper.h
            streaming/video/ffmpeg-renderers/pacer/pacer.h
        )

        if(NOT DISABLE_LIBVA)
            pkg_check_modules(LIBVA IMPORTED_TARGET libva)
            if(LIBVA_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBVA)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBVA)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/vaapi.cpp
                    streaming/video/ffmpeg-renderers/vaapi.h
                )

                if(NOT DISABLE_X11)
                    pkg_check_modules(LIBVA_X11 IMPORTED_TARGET libva-x11)
                    if(LIBVA_X11_FOUND)
                        target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBVA_X11)
                        target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBVA_X11)
                    endif()
                endif()

                if(NOT DISABLE_WAYLAND)
                    pkg_check_modules(LIBVA_WAYLAND IMPORTED_TARGET libva-wayland)
                    if(LIBVA_WAYLAND_FOUND)
                        target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBVA_WAYLAND)
                        target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBVA_WAYLAND)
                    endif()
                endif()

                if(NOT DISABLE_LIBDRM)
                    pkg_check_modules(LIBVA_DRM IMPORTED_TARGET libva-drm)
                    if(LIBVA_DRM_FOUND)
                        target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBVA_DRM)
                        target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBVA_DRM)
                    endif()
                endif()
            endif()
        endif()

        if(NOT DISABLE_LIBVDPAU)
            pkg_check_modules(VDPAU vdpau)
            if(VDPAU_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBVDPAU)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/vdpau.cpp
                    streaming/video/ffmpeg-renderers/vdpau.h
                )
            endif()
        endif()

        if(NOT DISABLE_MMAL)
            pkg_check_modules(MMAL IMPORTED_TARGET mmal)
            if(MMAL_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_MMAL)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::MMAL)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/mmal.cpp
                    streaming/video/ffmpeg-renderers/mmal.h
                )

                if(ALLOW_EGL_WITH_MMAL)
                    target_compile_definitions(${APP_TARGET} PRIVATE ALLOW_EGL_WITH_MMAL)
                endif()
            endif()
        endif()

        if(NOT DISABLE_LIBDRM)
            pkg_check_modules(LIBDRM IMPORTED_TARGET libdrm)
            if(LIBDRM_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_DRM)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBDRM)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/drm.cpp
                    streaming/video/ffmpeg-renderers/drm.h
                )

                if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DISABLE_MASTERHOOKS)
                    find_package(Threads REQUIRED)
                    target_compile_definitions(${APP_TARGET} PRIVATE HAVE_DRM_MASTER_HOOKS)
                    target_sources(${APP_TARGET} PRIVATE masterhook.c masterhook_internal.c)
                    target_link_libraries(${APP_TARGET} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
                endif()
            endif()
        endif()

        if(ENABLE_CUDA)
            pkg_check_modules(FFNVCODEC IMPORTED_TARGET ffnvcodec)
            if(FFNVCODEC_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_CUDA)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::FFNVCODEC ${CMAKE_DL_LIBS})
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/cuda.cpp
                    streaming/video/ffmpeg-renderers/cuda.h
                )
            endif()
        endif()

        if(NOT DISABLE_LIBPLACEBO)
            pkg_check_modules(LIBPLACEBO IMPORTED_TARGET libplacebo)
            if(LIBPLACEBO_FOUND)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_LIBPLACEBO_VULKAN)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::LIBPLACEBO)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/plvk.cpp
                    streaming/video/ffmpeg-renderers/plvk_c.c
                    streaming/video/ffmpeg-renderers/plvk.h
                )
            endif()
        endif()

        pkg_check_modules(EGL IMPORTED_TARGET egl)
        if(EGL_FOUND)
            include(CheckCXXSourceCompiles)
            set(CMAKE_REQUIRED_INCLUDES ${SDL2_INCLUDE_DIRS} ${EGL_INCLUDE_DIRS} ${FFMPEG_INCLUDE_DIRS})
            set(CMAKE_REQUIRED_LIBRARIES ${SDL2_LIBRARIES} ${EGL_LIBRARIES} ${FFMPEG_LIBRARIES})
            check_cxx_source_compiles("
                #define SDL_USE_BUILTIN_OPENGL_DEFINITIONS 1
                #include <SDL_egl.h>
                #include <SDL_opengles2.h>
                #ifndef EGL_VERSION_1_4
                #error EGLRenderer requires EGL 1.4
                #endif
                #ifndef GL_ES_VERSION_2_0
                #error EGLRenderer requires OpenGL ES 2.0
                #endif
                int main() { return 0; }
            " HAVE_EGL_SUPPORT)
            unset(CMAKE_REQUIRED_INCLUDES)
            unset(CMAKE_REQUIRED_LIBRARIES)

            if(HAVE_EGL_SUPPORT)
                target_compile_definitions(${APP_TARGET} PRIVATE HAVE_EGL)
                target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::EGL)
                target_sources(${APP_TARGET} PRIVATE
                    streaming/video/ffmpeg-renderers/eglvid.cpp
                    streaming/video/ffmpeg-renderers/egl_extensions.cpp
                    streaming/video/ffmpeg-renderers/eglimagefactory.cpp
                    streaming/video/ffmpeg-renderers/eglvid.h
                    streaming/video/ffmpeg-renderers/eglimagefactory.h
                )
            endif()
        endif()
    endif()

    if(NOT DISABLE_WAYLAND)
        pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client)
        if(WAYLAND_CLIENT_FOUND)
            target_compile_definitions(${APP_TARGET} PRIVATE HAS_WAYLAND)
            target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::WAYLAND_CLIENT)
            target_sources(${APP_TARGET} PRIVATE
                streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.cpp
                streaming/video/ffmpeg-renderers/pacer/waylandvsyncsource.h
            )
        endif()
    endif()

    if(NOT DISABLE_X11)
        pkg_check_modules(X11 IMPORTED_TARGET x11)
        if(X11_FOUND)
            target_compile_definitions(${APP_TARGET} PRIVATE HAS_X11)
            target_link_libraries(${APP_TARGET} PRIVATE PkgConfig::X11)
        endif()
    endif()
endif()

if(EMBEDDED)
    target_compile_definitions(${APP_TARGET} PRIVATE EMBEDDED_BUILD)
endif()
if(GLSLOW)
    target_compile_definitions(${APP_TARGET} PRIVATE GL_IS_SLOW)
endif()
if(VKSLOW)
    target_compile_definitions(${APP_TARGET} PRIVATE VULKAN_IS_SLOW)
endif()
if(GPUSLOW)
    target_compile_definitions(${APP_TARGET} PRIVATE GL_IS_SLOW VULKAN_IS_SLOW)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS ${APP_TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES deploy/linux/com.moonlight_stream.Moonlight.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES res/moonlight.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES deploy/linux/com.moonlight_stream.Moonlight.appdata.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
