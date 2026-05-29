cmake_minimum_required(VERSION 3.20)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "" FORCE)
endif()
project(gltfMaterialExample)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../core")
endif()

include(${TRUSSC_DIR}/cmake/trussc_app.cmake)
trussc_app()
