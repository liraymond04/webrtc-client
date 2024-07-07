# Findlibdatachannel.cmake
#
# This module locates the libdatachannel library.
# It sets the following variables:
#
# libdatachannel_FOUND - Set to true if the library is found.
# libdatachannel_INCLUDE_DIRS - List of directories containing headers.
# libdatachannel_LIBRARIES - Libraries to link against.

find_path(libdatachannel_INCLUDE_DIR
  NAMES rtc/rtc.h
  PATHS
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-src/include
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-build/include
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-src
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-build
    /usr/local/include
    /usr/include
)

find_library(libdatachannel_LIBRARY
  NAMES datachannel
  PATHS
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-build/lib
    ${CMAKE_BINARY_DIR}/_deps/libdatachannel-build
    /usr/local/lib
    /usr/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libdatachannel
  REQUIRED_VARS libdatachannel_INCLUDE_DIR libdatachannel_LIBRARY
  VERSION_VAR libdatachannel_VERSION
)

if(libdatachannel_FOUND)
  set(libdatachannel_INCLUDE_DIRS ${libdatachannel_INCLUDE_DIR})
  set(libdatachannel_LIBRARIES ${libdatachannel_LIBRARY})
endif()
