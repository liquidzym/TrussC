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
# Fetch HarfBuzz
# ---------------------------------------------------------------------------
include(FetchContent)

# Shaping-only: disable ALL optional backends and utilities.
# This minimizes compile time and avoids platform-specific dependencies.
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

# Emscripten: HarfBuzz needs pthreads disabled in some configs
if(EMSCRIPTEN)
    set(HB_HAVE_PTHREAD OFF CACHE BOOL "" FORCE)
endif()

# MSVC: suppress noisy HarfBuzz warnings
if(MSVC)
    set(HB_HAVE_VISIBILITY OFF CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
    harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    GIT_TAG 10.4.0
)
FetchContent_MakeAvailable(harfbuzz)

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
    harfbuzz
)

# C++17 for std::function, unordered_map, string_view
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_17)

# Windows: define NOMINMAX to prevent min/max macro collisions
if(WIN32)
    target_compile_definitions(${ADDON_NAME} PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

message(STATUS "[${ADDON_NAME}] Addon loaded (HarfBuzz)")
