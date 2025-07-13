# SDL2 CMake configuration file:
# This file is meant to be placed in Resources/CMake of a SDL2 framework

# INTERFACE_LINK_OPTIONS needs CMake 3.12
cmake_minimum_required(VERSION 3.12)

include(FeatureSummary)
set_package_properties(SDL2 PROPERTIES
    URL "https://www.libsdl.org/"
    DESCRIPTION "low level access to audio, keyboard, mouse, joystick, and graphics hardware"
)

# Copied from `configure_package_config_file`
macro(set_and_check _var _file)
    set(${_var} "${_file}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
    endif()
endmacro()

# Copied from `configure_package_config_file`
macro(check_required_components _NAME)
    foreach(comp ${${_NAME}_FIND_COMPONENTS})
        if(NOT ${_NAME}_${comp}_FOUND)
            if(${_NAME}_FIND_REQUIRED_${comp})
                set(${_NAME}_FOUND FALSE)
            endif()
        endif()
    endforeach()
endmacro()

set(SDL2_FOUND TRUE)

# Compute the installation prefix relative to this file.
set(SDL2_FRAMEWORK_PATH "${CMAKE_CURRENT_LIST_DIR}")                                # > /SDL2.framework/Resources/CMake/
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" REALPATH)       # > /SDL2.framework/Versions/Current/Resources/CMake
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" REALPATH)       # > /SDL2.framework/Versions/A/Resources/CMake/
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" PATH)           # > /SDL2.framework/Versions/A/Resources/
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" PATH)           # > /SDL2.framework/Versions/A/
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" PATH)           # > /SDL2.framework/Versions/
get_filename_component(SDL2_FRAMEWORK_PATH "${SDL2_FRAMEWORK_PATH}" PATH)           # > /SDL2.framework/
get_filename_component(SDL2_FRAMEWORK_PARENT_PATH "${SDL2_FRAMEWORK_PATH}" PATH)    # > /

# For compatibility with autotools sdl2-config.cmake, provide SDL2_* variables.

set_and_check(SDL2_PREFIX       "${SDL2_FRAMEWORK_PATH}")
set_and_check(SDL2_EXEC_PREFIX  "${SDL2_FRAMEWORK_PATH}")
set_and_check(SDL2_INCLUDE_DIR  "${SDL2_FRAMEWORK_PATH}/Headers")
set(SDL2_INCLUDE_DIRS           "${SDL2_INCLUDE_DIR};${SDL2_FRAMEWORK_PATH}")
set_and_check(SDL2_BINDIR       "${SDL2_FRAMEWORK_PATH}")
set_and_check(SDL2_LIBDIR       "${SDL2_FRAMEWORK_PATH}")

set(SDL2_LIBRARIES "SDL2::SDL2")

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated SDL2-target.cmake files.

if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 SHARED IMPORTED)
    set_target_properties(SDL2::SDL2
        PROPERTIES
            FRAMEWORK "TRUE"
            IMPORTED_LOCATION "${SDL2_FRAMEWORK_PATH}/Versions/A/SDL2"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
            COMPATIBLE_INTERFACE_BOOL "SDL2_SHARED"
            INTERFACE_SDL2_SHARED "ON"
            COMPATIBLE_INTERFACE_STRING "SDL_VERSION"
            INTERFACE_SDL_VERSION "SDL2"
    )
endif()
set(SDL2_SDL2_FOUND TRUE)

if(NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main INTERFACE IMPORTED)
endif()
set(SDL2_SDL2main_FOUND TRUE)

check_required_components(SDL2)
