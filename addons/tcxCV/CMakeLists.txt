# =============================================================================
# tcxCV - Computer Vision addon for TrussC (port of ofxCv)
# =============================================================================

cmake_minimum_required(VERSION 3.20)
set(ADDON_NAME tcxCV)

message(STATUS "[tcxCV] Loading addon...")

# Depend on tcxOpenCV (which fetches OpenCV)
# tcxCV extends tcxOpenCV with additional computer vision wrappers

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
