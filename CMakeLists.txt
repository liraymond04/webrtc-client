cmake_minimum_required(VERSION 3.11)
project(webrtc-client)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
include(FetchContent)

find_package(libdatachannel QUIET)
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

add_executable(${PROJECT_NAME} main.c)

target_link_libraries(${PROJECT_NAME} datachannel json-c uuid)
