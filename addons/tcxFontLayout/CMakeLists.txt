# =============================================================================
# tcxFontLayout — HarfBuzz text-shaping addon for TrussC
#
# Fetches HarfBuzz 10.4.0 as a static library (shaping-only — no FreeType,
# Graphite, ICU, or platform-specific backends).  TrussC uses stb_truetype
# for rasterization, so we only need HarfBuzz's core shaping engine.
#
# Cross-platform: macOS, iOS, Windows (MSVC), Linux, Android (NDK),
#                  Web (Emscripten).
# =============================================================================
cmake_minimum_required(VERSION 3.16)

set(ADDON_NAME tcxFontLayout)
message(STATUS "[${ADDON_NAME}] Loading addon...")

# ---------------------------------------------------------------------------
# HarfBuzz dependency
# ---------------------------------------------------------------------------
include(FetchContent)

if(EMSCRIPTEN)
    set(_TCX_FONTLAYOUT_PLATFORM "web")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_TCX_FONTLAYOUT_PLATFORM "ios")
elseif(ANDROID)
    set(_TCX_FONTLAYOUT_PLATFORM "android")
elseif(APPLE)
    set(_TCX_FONTLAYOUT_PLATFORM "macos")
elseif(WIN32)
    set(_TCX_FONTLAYOUT_PLATFORM "windows")
else()
    set(_TCX_FONTLAYOUT_PLATFORM "linux")
endif()

set(_TCX_FONTLAYOUT_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_fcache")
set(_TCX_FONTLAYOUT_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build-${_TCX_FONTLAYOUT_PLATFORM}")
option(TCX_FONTLAYOUT_USE_SYSTEM_HARFBUZZ "Use an installed HarfBuzz package instead of the addon cache" OFF)

# Shaping-only: disable ALL optional backends and utilities.
# This minimizes compile time and avoids platform-specific dependencies.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(HB_HAVE_FREETYPE    OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GRAPHITE2   OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GLIB        OFF CACHE BOOL "" FORCE)
set(HB_HAVE_ICU         OFF CACHE BOOL "" FORCE)
set(HB_HAVE_UNISCRIBE   OFF CACHE BOOL "" FORCE)
set(HB_HAVE_DIRECTWRITE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_CORETEXT    OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GDI         OFF CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET     OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS      OFF CACHE BOOL "" FORCE)
set(HB_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(HB_BUILD_DOCS       OFF CACHE BOOL "" FORCE)
set(HB_BUILD_GOBJECT    OFF CACHE BOOL "" FORCE)
set(HB_BUILD_INTROSPECTION OFF CACHE BOOL "" FORCE)

# Emscripten: HarfBuzz needs pthreads disabled in some configs
if(EMSCRIPTEN)
    set(HB_HAVE_PTHREAD OFF CACHE BOOL "" FORCE)
endif()

# MSVC: suppress noisy HarfBuzz warnings
if(MSVC)
    set(HB_HAVE_VISIBILITY OFF CACHE BOOL "" FORCE)
endif()

set(_TCX_FONTLAYOUT_HARFBUZZ_TARGET harfbuzz)
if(TCX_FONTLAYOUT_USE_SYSTEM_HARFBUZZ)
    find_package(harfbuzz CONFIG QUIET)
    if(TARGET harfbuzz::harfbuzz)
        set(_TCX_FONTLAYOUT_HARFBUZZ_TARGET harfbuzz::harfbuzz)
    elseif(TARGET harfbuzz)
        set(_TCX_FONTLAYOUT_HARFBUZZ_TARGET harfbuzz)
    endif()
endif()

if(NOT TARGET ${_TCX_FONTLAYOUT_HARFBUZZ_TARGET})
    FetchContent_Declare(
        harfbuzz
        GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
        GIT_TAG 10.4.0
        GIT_SHALLOW TRUE
        UPDATE_DISCONNECTED TRUE
        SOURCE_DIR "${_TCX_FONTLAYOUT_CACHE_DIR}/harfbuzz-src"
        BINARY_DIR "${_TCX_FONTLAYOUT_BUILD_ROOT}/harfbuzz-build"
        SUBBUILD_DIR "${_TCX_FONTLAYOUT_BUILD_ROOT}/harfbuzz-subbuild"
    )
    FetchContent_MakeAvailable(harfbuzz)
    set(_TCX_FONTLAYOUT_HARFBUZZ_TARGET harfbuzz)
endif()

# ---------------------------------------------------------------------------
# Addon library
# ---------------------------------------------------------------------------
file(GLOB ADDON_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB ADDON_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS})

target_include_directories(${ADDON_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${ADDON_NAME} PUBLIC
    TrussC
    ${_TCX_FONTLAYOUT_HARFBUZZ_TARGET}
)

# C++17 for std::function, unordered_map, string_view
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_17)

# Windows: define NOMINMAX to prevent min/max macro collisions
if(WIN32)
    target_compile_definitions(${ADDON_NAME} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

message(STATUS "[${ADDON_NAME}] Addon loaded (HarfBuzz)")
