# ##############################################################################
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ##############################################################################

# * Config file for the VPL package It defines the following variables
#   VPL_<component>_FOUND VPL_IMPORTED_TARGETS
#
# VPLConfigVersion.cmake defines VPL_VERSION

option(VPL_SHARED "Use shared instead of static version of dispatcher."
       ON)

# Initialize to default values
set(VPL_INTERFACE_VERSION 2)
get_filename_component(_vpl_config_file
                       "${CMAKE_CURRENT_LIST_DIR}/VPLConfig.cmake" REALPATH)
get_filename_component(_vpl_config_dir "${_vpl_config_file}" DIRECTORY)
get_filename_component(_vpl_install_prefix
                       "${_vpl_config_dir}/../../../" REALPATH)

get_filename_component(VPL_LIB_DIR "${_vpl_config_dir}/../../"
                       ABSOLUTE)
get_filename_component(VPL_INCLUDE_DIR "${_vpl_config_dir}/../../../include"
                       ABSOLUTE)
get_filename_component(VPL_BIN_DIR "${_vpl_config_dir}/../../../bin"
                       ABSOLUTE)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  set(VPL_SHLIB_DIR ${VPL_BIN_DIR})
else()
  set(VPL_SHLIB_DIR ${VPL_LIB_DIR})
endif()

if(NOT VPL_IMPORTED_TARGETS)
  set(VPL_IMPORTED_TARGETS "")
endif()

if(NOT VPL_FIND_COMPONENTS)
  set(VPL_FIND_COMPONENTS "dispatcher;api")
  foreach(_vpl_component ${VPL_FIND_COMPONENTS})
    set(VPL_FIND_REQUIRED_${_vpl_component} 1)
  endforeach()
endif()

# VPL::dispatcher
set(VPL_dispatcher_FOUND 0)

get_filename_component(_dispatcher_shlib "${VPL_SHLIB_DIR}/libvpl.dll"
                       ABSOLUTE)
get_filename_component(_dispatcher_debug_shlib
                       "${VPL_SHLIB_DIR}/libvpld.dll" ABSOLUTE)
get_filename_component(_dispatcher_lib "${VPL_LIB_DIR}/vpl.lib"
                       ABSOLUTE)
get_filename_component(_dispatcher_debug_lib
                       "${VPL_LIB_DIR}/vpld.lib" ABSOLUTE)
get_filename_component(_dispatcher_implib "${VPL_LIB_DIR}/vpl.lib"
                       ABSOLUTE)
get_filename_component(_dispatcher_debug_implib
                       "${VPL_LIB_DIR}/vpld.lib" ABSOLUTE)

if(TARGET VPL::dispatcher)
  list(APPEND VPL_IMPORTED_TARGETS VPL::dispatcher)
  set(VPL_dispatcher_FOUND 1)
else()
  if(VPL_SHARED)
    if(EXISTS "${_dispatcher_shlib}" OR EXISTS "${_dispatcher_debug_shlib}")
      list(APPEND VPL_IMPORTED_TARGETS VPL::dispatcher)
      set(VPL_dispatcher_FOUND 1)

      add_library(VPL::dispatcher SHARED IMPORTED)
      set_target_properties(
        VPL::dispatcher
        PROPERTIES IMPORTED_LOCATION_RELEASE ${_dispatcher_shlib}
                   IMPORTED_LOCATION_RELWITHDEBINFO ${_dispatcher_shlib}
                   IMPORTED_LOCATION_MINSIZEREL ${_dispatcher_shlib}
                   IMPORTED_LOCATION_DEBUG ${_dispatcher_debug_shlib}
                   IMPORTED_LOCATION_RELWITHDEBRT ${_dispatcher_debug_shlib})
      if(CMAKE_SYSTEM_NAME MATCHES Windows)
        set_target_properties(
          VPL::dispatcher
          PROPERTIES IMPORTED_IMPLIB_RELEASE ${_dispatcher_implib}
                     IMPORTED_IMPLIB_RELWITHDEBINFO ${_dispatcher_implib}
                     IMPORTED_IMPLIB_MINSIZEREL ${_dispatcher_implib}
                     IMPORTED_IMPLIB_DEBUG ${_dispatcher_debug_implib}
                     IMPORTED_IMPLIB_RELWITHDEBRT ${_dispatcher_debug_implib})
      endif()
    elseif(VPL_FIND_REQUIRED AND VPL_FIND_REQUIRED_dispatcher)
      message(STATUS "Unable to find required VPL component: dispatcher")
      set(VPL_FOUND FALSE)
    endif()
  else()
    if(EXISTS "${_dispatcher_lib}" OR EXISTS "${_dispatcher_debug_lib}")
      list(APPEND VPL_IMPORTED_TARGETS VPL::dispatcher)
      set(VPL_dispatcher_FOUND 1)
      add_library(VPL::dispatcher STATIC IMPORTED)
      set_target_properties(
        VPL::dispatcher
        PROPERTIES IMPORTED_LOCATION_RELEASE ${_dispatcher_lib}
                   IMPORTED_LOCATION_RELWITHDEBINFO ${_dispatcher_lib}
                   IMPORTED_LOCATION_MINSIZEREL ${_dispatcher_lib}
                   IMPORTED_LOCATION_DEBUG ${_dispatcher_debug_lib}
                   IMPORTED_LOCATION_RELWITHDEBRT ${_dispatcher_debug_lib})
      if(UNIX)
        # require pthreads for loading legacy MSDK runtimes
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
        set(THREADS_PREFER_PTHREAD_FLAG TRUE)
        find_package(Threads REQUIRED)
        target_link_libraries(VPL::dispatcher INTERFACE Threads::Threads)
      endif()
      target_link_libraries(VPL::dispatcher INTERFACE ${CMAKE_DL_LIBS})
    elseif(VPL_FIND_REQUIRED AND VPL_FIND_REQUIRED_dispatcher)
      message(STATUS "Unable to find required VPL component: dispatcher")
      set(VPL_FOUND FALSE)
    endif()
  endif()
  set_target_properties(VPL::dispatcher PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                   "${VPL_INCLUDE_DIR}")
endif()

unset(_dispatcher_shlib)

# VPL::api
set(VPL_api_FOUND 0)
if(EXISTS ${VPL_INCLUDE_DIR})
  if(NOT TARGET VPL::api)
    add_library(VPL::api INTERFACE IMPORTED)
    set_target_properties(VPL::api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                              "${VPL_INCLUDE_DIR}")
  endif()
  list(APPEND VPL_IMPORTED_TARGETS VPL::api)
  set(VPL_api_FOUND 1)
endif()

# VPL::cppapi
set(VPL_cppapi_FOUND 0)
if(EXISTS ${VPL_INCLUDE_DIR})
  if(NOT TARGET VPL::cppapi)
    add_library(VPL::cppapi INTERFACE IMPORTED)
    set_target_properties(VPL::cppapi PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                 "${VPL_INCLUDE_DIR}")
  endif()
  list(APPEND VPL_IMPORTED_TARGETS VPL::cppapi)
  set(VPL_cppapi_FOUND 1)
endif()
