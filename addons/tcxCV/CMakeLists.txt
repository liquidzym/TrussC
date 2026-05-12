# =============================================================================
# tcxCV - Computer Vision addon for TrussC (port of ofxCv)
# =============================================================================

cmake_minimum_required(VERSION 3.20)
set(ADDON_NAME tcxCV)

message(STATUS "[tcxCV] Loading addon...")

# Depend on tcxOpenCV (which fetches OpenCV)
# tcxCV extends tcxOpenCV with additional computer vision wrappers
if(NOT TARGET tcxOpenCV)
    get_filename_component(_TCX_CV_ADDONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
    set(_TCX_CV_OPENCV_DIR "${_TCX_CV_ADDONS_DIR}/tcxOpenCV")
    if(NOT EXISTS "${_TCX_CV_OPENCV_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "[tcxCV] Required addon tcxOpenCV was not found at ${_TCX_CV_OPENCV_DIR}")
    endif()

    add_subdirectory("${_TCX_CV_OPENCV_DIR}" "${CMAKE_BINARY_DIR}/addons/tcxOpenCV")

    # Keep TrussC's addon loader cache in sync when tcxCV loads its dependency.
    # This prevents duplicate add_subdirectory() calls if addons.make also lists tcxOpenCV.
    list(FIND _TC_LOADED_ADDONS tcxOpenCV _TCX_CV_OPENCV_INDEX)
    if(_TCX_CV_OPENCV_INDEX EQUAL -1)
        list(APPEND _TC_LOADED_ADDONS tcxOpenCV)
        set(_TC_LOADED_ADDONS "${_TC_LOADED_ADDONS}" CACHE INTERNAL "List of loaded TrussC addons")
    endif()
endif()

# Source files
file(GLOB TCX_CV_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cld/*.cpp"
)

file(GLOB TCX_CV_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cld/*.h"
)

add_library(${ADDON_NAME} STATIC ${TCX_CV_SOURCES} ${TCX_CV_HEADERS})

target_include_directories(${ADDON_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/cld"
)

# Link to tcxOpenCV (which provides OpenCV and conversion functions)
target_link_libraries(${ADDON_NAME} PUBLIC tcxOpenCV TrussC)

# Note: OpenCV include directories are inherited from tcxOpenCV via PUBLIC link

message(STATUS "[tcxCV] Addon loaded")
