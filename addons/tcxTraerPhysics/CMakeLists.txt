# =============================================================================
# tcxTraerPhysics - TrussC Particle Physics Addon (header-only)
# =============================================================================

cmake_minimum_required(VERSION 3.16)

set(ADDON_NAME tcxTraerPhysics)

# Header-only — no source compilation needed
add_library(${ADDON_NAME} INTERFACE)

target_include_directories(${ADDON_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${ADDON_NAME} INTERFACE TrussC)
