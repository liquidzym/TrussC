cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "" FORCE)
endif()
project(verifyModelExample LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../core")
endif()

if(EMSCRIPTEN)
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-web")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-ios")
elseif(ANDROID)
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-android")
elseif(APPLE)
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-macos")
elseif(WIN32)
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-windows")
else()
    set(_TC_BUILD_DIR "${TRUSSC_DIR}/build-linux")
endif()

if(NOT TARGET TrussC)
    add_subdirectory("${TRUSSC_DIR}" "${_TC_BUILD_DIR}")
endif()

set(_ADDON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
add_subdirectory("${_ADDON_DIR}" "${CMAKE_BINARY_DIR}/addons/tcxAssimp")

add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE tcxAssimp TrussC)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TCX_ASSIMP_VERIFY_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    TCX_ASSIMP_SAMPLE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../staticModelExample/bin/data"
)
