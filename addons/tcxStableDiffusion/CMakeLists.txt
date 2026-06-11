cmake_minimum_required(VERSION 3.20)
project(tcxStableDiffusion LANGUAGES CXX)

set(ADDON_NAME tcxStableDiffusion)
message(STATUS "[tcxStableDiffusion] Loading addon...")

if(NOT WIN32 AND NOT APPLE)
    message(FATAL_ERROR "[tcxStableDiffusion] Only Windows and macOS are supported.")
endif()

file(GLOB_RECURSE TCXSD_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)
file(GLOB_RECURSE TCXSD_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${ADDON_NAME} STATIC
    ${TCXSD_HEADERS}
    ${TCXSD_SOURCES}
)
add_library(tc::tcxStableDiffusion ALIAS ${ADDON_NAME})

target_include_directories(${ADDON_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_20)

if(TARGET TrussC)
    target_link_libraries(${ADDON_NAME} PUBLIC TrussC)
else()
    set(_TCXSD_TRUSSC_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../../core")
    target_include_directories(${ADDON_NAME} PUBLIC "${_TCXSD_TRUSSC_CORE}/include")
endif()

set(TCXSD_PATHS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/libs/stable-diffusion/current/tcxStableDiffusionPaths.cmake")
if(EXISTS "${TCXSD_PATHS_FILE}")
    include("${TCXSD_PATHS_FILE}")
    set(TCXSD_HAS_NATIVE_VALUE 1)
    if(NOT DEFINED TCXSD_NATIVE_CLI)
        set(TCXSD_NATIVE_CLI "")
    endif()
else()
    set(TCXSD_HAS_NATIVE_VALUE 0)
    set(TCXSD_NATIVE_CLI "")
    if(WIN32)
        set(_TCXSD_SETUP_PROFILE "windows-cuda")
    else()
        set(_TCXSD_SETUP_PROFILE "macos-metal")
    endif()
    message(WARNING
        "[tcxStableDiffusion] stable-diffusion.cpp runtime is not setup. Run:\n"
        "  python ${CMAKE_CURRENT_SOURCE_DIR}/tools/setup_sd.py build-native --profile ${_TCXSD_SETUP_PROFILE}")
endif()

target_compile_definitions(${ADDON_NAME} PUBLIC
    TCXSD_HAS_NATIVE=${TCXSD_HAS_NATIVE_VALUE}
    TCXSD_ADDON_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    TCXSD_NATIVE_CLI="${TCXSD_NATIVE_CLI}"
)

if(TCXSD_HAS_NATIVE_VALUE)
    target_compile_definitions(${ADDON_NAME} PRIVATE SD_BUILD_SHARED_LIB)
    target_include_directories(${ADDON_NAME} PRIVATE "${TCXSD_NATIVE_INCLUDE_DIR}")

    if(TCXSD_NATIVE_LIBRARY)
        target_link_libraries(${ADDON_NAME} PRIVATE "${TCXSD_NATIVE_LIBRARY}")
    else()
        message(WARNING "[tcxStableDiffusion] Native runtime path file did not provide TCXSD_NATIVE_LIBRARY.")
    endif()

    if(COMMAND tc_addon_bundle_file)
        foreach(_TCXSD_RUNTIME_FILE IN LISTS TCXSD_NATIVE_RUNTIME_FILES)
            if(EXISTS "${_TCXSD_RUNTIME_FILE}")
                tc_addon_bundle_file("${_TCXSD_RUNTIME_FILE}")
            endif()
        endforeach()
    endif()
endif()

message(STATUS "[tcxStableDiffusion] Addon loaded")
