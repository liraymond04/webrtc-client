cmake_minimum_required(VERSION 3.11)
project(rtc_handler)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default options
option(BUILD_STATIC_LIBS "Build example executables" ON)
set(STATIC ${BUILD_STATIC_LIBS})
option(BUILD_EXAMPLES "Build example executables" ON)
set(EXAMPLES ${BUILD_EXAMPLES})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Dependencies
include(FetchContent)

find_package(libdatachannel)
if (NOT libdatachannel_FOUND)
    FetchContent_Declare(
        libdatachannel
        GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
        GIT_TAG 0f7985f4662a9664d7567eea2500ef9c190d1cbc
        GIT_PROGRESS TRUE
        GIT_SHALLOW false
    )
    FetchContent_GetProperties(libdatachannel)
    if (NOT libdatachannel_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_Populate(libdatachannel)
        set(PREFER_SYSTEM_LIB OFF CACHE BOOL "" FORCE)
        set(NO_EXAMPLES ON CACHE BOOL "" FORCE) 
        set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
        set(USE_NICE OFF CACHE BOOL "" FORCE)
        set(CMAKE_BUILD_TYPE "Release")
        add_subdirectory(${libdatachannel_SOURCE_DIR} ${libdatachannel_BINARY_DIR})
    endif()
endif()

find_package(json-c QUIET)
if (NOT json-C_FOUND)
    FetchContent_Declare(
        json-c
        GIT_REPOSITORY https://github.com/json-c/json-c.git
        GIT_TAG 6e481aa06166c2b31e391005c6e50f0adef5e4d4
        GIT_PROGRESS TRUE
        GIT_SHALLOW false
    )
    FetchContent_GetProperties(json-c)
    if (NOT json-c_POPULATED)
        set(FETCHCONTENT_QUIET NO)
        FetchContent_Populate(json-c)
        set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(${json-c_SOURCE_DIR} ${json-c_BINARY_DIR})
    endif()
endif()

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

if (STATIC)
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
else()
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} datachannel json-c uuid ncurses)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.c")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

# Build examples
if (EXAMPLES)
    foreach(SOURCE_FILE ${SRC_FILES})
        get_filename_component(EXE_NAME ${SOURCE_FILE} NAME_WE)
        add_executable(${EXE_NAME} ${SOURCE_FILE})
        target_include_directories(${EXE_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(${EXE_NAME} ${PROJECT_NAME})
    endforeach()
endif()
