# =============================================================================
# tcxFontLayout — HarfBuzz text-shaping addon for TrussC
#
# Fetches HarfBuzz as a static library (shaping-only, no FreeType needed —
# TrussC uses stb_truetype for rasterization).
#
# Cross-platform: macOS, Windows (MSVC), Linux, Web (Emscripten).
# =============================================================================
cmake_minimum_required(VERSION 3.16)

set(ADDON_NAME tcxFontLayout)
message(STATUS "[${ADDON_NAME}] Loading addon...")

# ---------------------------------------------------------------------------
# Fetch HarfBuzz (shaping-only — no FreeType dependency)
# ---------------------------------------------------------------------------
include(FetchContent)

# Disable HarfBuzz features we don't need
set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GRAPHITE2 OFF CACHE BOOL "" FORCE)
set(HB_HAVE_GLIB OFF CACHE BOOL "" FORCE)
set(HB_HAVE_ICU OFF CACHE BOOL "" FORCE)
set(HB_HAVE_UNISCRIBE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_DIRECTWRITE OFF CACHE BOOL "" FORCE)
set(HB_HAVE_CORETEXT OFF CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

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

# C++17 for string_view, optional, etc.
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_17)

message(STATUS "[${ADDON_NAME}] Addon loaded (HarfBuzz ${harfbuzz_VERSION})")
