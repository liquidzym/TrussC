cmake_minimum_required(VERSION 3.20)
project(tcxMapWrap LANGUAGES C CXX)

message(STATUS "[tcxMapWrap] Loading addon...")

# =============================================================================
# tcxMapWrap — TrussC Projection Mapping Addon
# Platforms: macOS / iOS / Windows only
# =============================================================================

file(GLOB_RECURSE TCX_MAPWRAP_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE TCX_MAPWRAP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(tcxMapWrap STATIC
    ${TCX_MAPWRAP_HEADERS}
    ${TCX_MAPWRAP_SOURCES}
)

target_include_directories(tcxMapWrap PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(tcxMapWrap PUBLIC cxx_std_20)

# --- TrussC linkage ---
if(TARGET TrussC)
    target_link_libraries(tcxMapWrap PUBLIC TrussC)
    target_compile_definitions(tcxMapWrap PUBLIC TCX_MAPWRAP_HAS_TRUSSC_RUNTIME=1)
    # Get TrussC include dirs for nlohmann/json etc.
    get_target_property(TRUSSC_INC TrussC INTERFACE_INCLUDE_DIRECTORIES)
    if(TRUSSC_INC)
        target_include_directories(tcxMapWrap PUBLIC ${TRUSSC_INC})
    endif()
else()
    target_compile_definitions(tcxMapWrap PUBLIC TCX_MAPWRAP_HAS_TRUSSC_RUNTIME=0)
    # Standalone build: add TrussC core include path for nlohmann/json
    set(TRUSSC_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../core")
    if(EXISTS "${TRUSSC_CORE_DIR}/include")
        target_include_directories(tcxMapWrap PUBLIC "${TRUSSC_CORE_DIR}/include")
    endif()
endif()

# --- Platform-specific linkage ---
if(WIN32)
    # Needed for GetUserDefaultUILanguage in i18n
    target_link_libraries(tcxMapWrap PUBLIC kernel32 user32)
endif()

if(APPLE)
    # Needed for NSLocale in i18n
    target_link_libraries(tcxMapWrap PUBLIC "-framework Foundation")
    # Enable Objective-C++ for i18n system locale detection
    enable_language(OBJCXX)
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/i18n/MapWrapI18n.cpp"
        PROPERTIES LANGUAGE OBJCXX
    )
endif()

option(TCX_MAPWRAP_BUILD_TESTS "Build tcxMapWrap unit tests" OFF)

if(TCX_MAPWRAP_BUILD_TESTS)
    file(GLOB TCX_MAPWRAP_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )

    add_executable(tcxMapWrap_tests ${TCX_MAPWRAP_TEST_SOURCES})
    target_link_libraries(tcxMapWrap_tests PRIVATE tcxMapWrap)
    target_compile_features(tcxMapWrap_tests PRIVATE cxx_std_20)

    enable_testing()
    add_test(NAME tcxMapWrap_tests COMMAND tcxMapWrap_tests)
endif()

message(STATUS "[tcxMapWrap] Addon loaded")
