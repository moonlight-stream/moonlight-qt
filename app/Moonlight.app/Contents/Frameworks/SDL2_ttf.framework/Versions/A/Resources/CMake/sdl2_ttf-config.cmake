# SDL2_ttf CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL2_ttf framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

include(FeatureSummary)
set_package_properties(SDL2_ttf PROPERTIES
    URL "https://www.libsdl.org/projects/SDL_ttf/"
    DESCRIPTION "Support for TrueType (.ttf) font files with Simple Directmedia Layer"
)

set(SDL2_ttf_FOUND TRUE)

set(SDL2TTF_VENDORED TRUE)

set(SDL2TTF_HARFBUZZ TRUE)
set(SDL2TTF_FREETYPE TRUE)

string(REGEX REPLACE "SDL2_ttf\\.framework.*" "SDL2_ttf.framework" _sdl2ttf_framework_path "${CMAKE_CURRENT_LIST_DIR}")
string(REGEX REPLACE "SDL2_ttf\\.framework.*" "" _sdl2ttf_framework_parent_path "${CMAKE_CURRENT_LIST_DIR}")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2_ttf-target.cmake files.

if(NOT TARGET SDL2_ttf::SDL2_ttf)
    add_library(SDL2_ttf::SDL2_ttf INTERFACE IMPORTED)
    set_target_properties(SDL2_ttf::SDL2_ttf
        PROPERTIES
            INTERFACE_COMPILE_OPTIONS "SHELL:-F \"${_sdl2ttf_framework_parent_path}\""
            INTERFACE_INCLUDE_DIRECTORIES "${_sdl2ttf_framework_path}/Headers"
            INTERFACE_LINK_OPTIONS "SHELL:-F \"${_sdl2ttf_framework_parent_path}\";SHELL:-framework SDL2_ttf"
            COMPATIBLE_INTERFACE_BOOL "SDL2_SHARED"
            INTERFACE_SDL2_SHARED "ON"
    )
endif()

unset(_sdl2ttf_framework_path)
unset(_sdl2ttf_framework_parent_path)
