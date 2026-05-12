# =============================================================================
# tcxOpenCV - OpenCV integration addon for TrussC
# =============================================================================

set(ADDON_NAME tcxOpenCV)

# -----------------------------------------------------------------------------
# OpenCV dependency
# -----------------------------------------------------------------------------
include(FetchContent)

option(TCX_OPENCV_USE_SYSTEM_OPENCV "Use an installed OpenCV package instead of the addon-local build cache" OFF)

set(_TCX_OPENCV_MODULES
    core
    imgproc
    imgcodecs
    videoio
    objdetect
    features2d
    calib3d
    video
    flann
)

set(_TCX_OPENCV_LINK_LIBS "")
set(_TCX_OPENCV_INCLUDE_DIRS "")
set(_TCX_OPENCV_USING_SYSTEM OFF)

if(TCX_OPENCV_USE_SYSTEM_OPENCV)
    find_package(OpenCV 4 CONFIG QUIET COMPONENTS ${_TCX_OPENCV_MODULES})
    if(OpenCV_FOUND)
        set(_TCX_OPENCV_USING_SYSTEM ON)
        set(_TCX_OPENCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS})
        set(_TCX_OPENCV_LINK_LIBS ${OpenCV_LIBS})
        message(STATUS "tcxOpenCV: Using system OpenCV ${OpenCV_VERSION}")
    else()
        message(WARNING "tcxOpenCV: TCX_OPENCV_USE_SYSTEM_OPENCV=ON but OpenCV was not found; falling back to addon-local OpenCV")
    endif()
endif()

if(NOT _TCX_OPENCV_USING_SYSTEM)
    if(APPLE)
        set(_TCX_OPENCV_PLATFORM "macos")
    elseif(WIN32)
        set(_TCX_OPENCV_PLATFORM "windows")
    elseif(UNIX)
        set(_TCX_OPENCV_PLATFORM "linux")
    else()
        set(_TCX_OPENCV_PLATFORM "${CMAKE_SYSTEM_NAME}")
    endif()

    string(TOLOWER "${_TCX_OPENCV_PLATFORM}" _TCX_OPENCV_PLATFORM)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _TCX_OPENCV_ARCH)
    set(_TCX_OPENCV_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_fcache")
    set(_TCX_OPENCV_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build-${_TCX_OPENCV_PLATFORM}-${_TCX_OPENCV_ARCH}")

    # Build static libraries (no dylib dependencies for distribution)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # OpenCV build options - minimize build time
    set(BUILD_LIST "core,imgproc,imgcodecs,videoio,objdetect,features2d,calib3d,video,flann" CACHE STRING "" FORCE)
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
        SOURCE_DIR "${_TCX_OPENCV_CACHE_DIR}/opencv-src"
        BINARY_DIR "${_TCX_OPENCV_BUILD_ROOT}/opencv-build"
        SUBBUILD_DIR "${_TCX_OPENCV_BUILD_ROOT}/opencv-subbuild"
        UPDATE_DISCONNECTED TRUE
    )

    # Suppress cmake_minimum_required deprecation from OpenCV internals (CMake 4.x)
    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "" FORCE)

    message(STATUS "tcxOpenCV: Preparing addon-local OpenCV cache at ${_TCX_OPENCV_BUILD_ROOT}")
    FetchContent_MakeAvailable(opencv)

    set(CMAKE_WARN_DEPRECATED ON CACHE BOOL "" FORCE)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS OFF CACHE BOOL "" FORCE)

    # Get OpenCV include directories from the generated config.
    set(_TCX_OPENCV_INCLUDE_DIRS
        ${CMAKE_BINARY_DIR}
        ${opencv_BINARY_DIR}
        ${opencv_SOURCE_DIR}/include
        ${opencv_SOURCE_DIR}/modules/core/include
        ${opencv_SOURCE_DIR}/modules/imgproc/include
        ${opencv_SOURCE_DIR}/modules/imgcodecs/include
        ${opencv_SOURCE_DIR}/modules/videoio/include
        ${opencv_SOURCE_DIR}/modules/objdetect/include
        ${opencv_SOURCE_DIR}/modules/features2d/include
        ${opencv_SOURCE_DIR}/modules/calib3d/include
        ${opencv_SOURCE_DIR}/modules/video/include
        ${opencv_SOURCE_DIR}/modules/flann/include
    )

    set(_TCX_OPENCV_LINK_LIBS
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
        opencv_objdetect
        opencv_features2d
        opencv_calib3d
        opencv_video
        opencv_flann
    )
endif()

# -----------------------------------------------------------------------------
# Addon library (INTERFACE - header-only, links to OpenCV)
# -----------------------------------------------------------------------------
add_library(${ADDON_NAME} INTERFACE)

# Include our headers + OpenCV headers
target_include_directories(${ADDON_NAME}
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${_TCX_OPENCV_INCLUDE_DIRS}
)

# Link to TrussC and OpenCV modules
target_link_libraries(${ADDON_NAME}
    INTERFACE
        TrussC
        ${_TCX_OPENCV_LINK_LIBS}
)

# Alias for consistent naming
add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
