# =============================================================================
# tcxOpenCV - OpenCV integration addon for TrussC
# =============================================================================

set(ADDON_NAME tcxOpenCV)

# -----------------------------------------------------------------------------
# FetchContent: OpenCV
# -----------------------------------------------------------------------------
include(FetchContent)

# Build static libraries (no dylib dependencies for distribution)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Match TrussC's dynamic MSVC runtime (/MD). OpenCV defaults its static libs to
# a static CRT (/MT) on Windows, which collides with the app's /MD runtime at
# link time (LNK2038 RuntimeLibrary mismatch -> LNK2005/LNK1169). Force OpenCV
# onto the dynamic CRT so both sides agree. No-op on non-MSVC toolchains.
set(BUILD_WITH_STATIC_CRT OFF CACHE BOOL "" FORCE)

# OpenCV build options - minimize build time
set(BUILD_LIST "core,imgproc,imgcodecs,videoio,objdetect,features2d,calib3d" CACHE STRING "" FORCE)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python_bindings_generator OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python_tests OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_java OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_js OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_world OFF CACHE BOOL "" FORCE)
set(WITH_FFMPEG OFF CACHE BOOL "" FORCE)  # Use native capture instead
set(WITH_GSTREAMER OFF CACHE BOOL "" FORCE)
set(WITH_1394 OFF CACHE BOOL "" FORCE)
set(WITH_V4L OFF CACHE BOOL "" FORCE)
set(WITH_CUDA OFF CACHE BOOL "" FORCE)
set(WITH_OPENCL OFF CACHE BOOL "" FORCE)
set(WITH_IPP OFF CACHE BOOL "" FORCE)
set(WITH_ITT OFF CACHE BOOL "" FORCE)
set(WITH_EIGEN OFF CACHE BOOL "" FORCE)
set(WITH_PROTOBUF OFF CACHE BOOL "" FORCE)
set(WITH_QUIRC OFF CACHE BOOL "" FORCE)
set(WITH_OPENEXR OFF CACHE BOOL "" FORCE)
set(WITH_TIFF OFF CACHE BOOL "" FORCE)
set(WITH_OPENJPEG OFF CACHE BOOL "" FORCE)
set(OPENCV_GENERATE_PKGCONFIG OFF CACHE BOOL "" FORCE)

# Use system zlib to avoid macOS SDK compatibility issues
set(BUILD_ZLIB OFF CACHE BOOL "" FORCE)

# Disable OBSENSOR backend (causes issues in 4.10.0)
set(WITH_OBSENSOR OFF CACHE BOOL "" FORCE)

# Platform-specific options
if(APPLE)
    set(WITH_AVFOUNDATION ON CACHE BOOL "" FORCE)
elseif(WIN32)
    set(WITH_MSMF ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.12.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

# Suppress cmake_minimum_required deprecation from OpenCV internals (CMake 4.x)
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

message(STATUS "tcxOpenCV: Fetching OpenCV (this may take a while on first build)...")
FetchContent_MakeAvailable(opencv)

set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS OFF CACHE BOOL "" FORCE)

# -----------------------------------------------------------------------------
# Addon library (INTERFACE - header-only, links to OpenCV)
# -----------------------------------------------------------------------------
add_library(${ADDON_NAME} INTERFACE)

# Get OpenCV include directories from the generated config
# Note: opencv_modules.hpp is generated in ${CMAKE_BINARY_DIR}/opencv2/
set(_OPENCV_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}
    ${opencv_SOURCE_DIR}/modules/core/include
    ${opencv_SOURCE_DIR}/modules/imgproc/include
    ${opencv_SOURCE_DIR}/modules/imgcodecs/include
    ${opencv_SOURCE_DIR}/modules/videoio/include
    ${opencv_SOURCE_DIR}/modules/objdetect/include
    ${opencv_SOURCE_DIR}/modules/features2d/include
    ${opencv_SOURCE_DIR}/modules/calib3d/include
    ${opencv_SOURCE_DIR}/modules/flann/include
    ${opencv_BINARY_DIR}
)

# Include our headers + OpenCV headers
target_include_directories(${ADDON_NAME}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_OPENCV_INCLUDE_DIRS}
)

# Link to TrussC and OpenCV modules
target_link_libraries(${ADDON_NAME}
    INTERFACE
        TrussC
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
        opencv_objdetect
        opencv_features2d
        opencv_calib3d
)

# Alias for consistent naming
add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
