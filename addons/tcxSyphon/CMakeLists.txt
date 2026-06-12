cmake_minimum_required(VERSION 3.20)
set(ADDON_NAME tcxSyphon)

# -----------------------------------------------------------------------------
# Syphon is macOS-only. On other platforms expose a header-only stub so user
# projects that list tcxSyphon still configure (the API becomes a no-op there).
# -----------------------------------------------------------------------------
if(NOT APPLE)
    add_library(${ADDON_NAME} INTERFACE)
    target_include_directories(${ADDON_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
    message(STATUS "[tcxSyphon] Non-macOS platform: building header-only stub")
    return()
endif()

enable_language(OBJC)
enable_language(OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(_SF "${CMAKE_CURRENT_SOURCE_DIR}/Syphon-Framework")

if(NOT EXISTS "${_SF}/SyphonMetalServer.h")
    message(FATAL_ERROR
        "[tcxSyphon] Syphon-Framework submodule is missing.\n"
        "  Run: git submodule update --init --recursive")
endif()

# -----------------------------------------------------------------------------
# Make `#import <Syphon/Foo.h>` resolve to the framework source tree by adding a
# symlinked include root (.../syphon_include/Syphon -> Syphon-Framework).
# -----------------------------------------------------------------------------
set(_SYPHON_INC "${CMAKE_CURRENT_BINARY_DIR}/syphon_include")
file(MAKE_DIRECTORY "${_SYPHON_INC}")
if(NOT EXISTS "${_SYPHON_INC}/Syphon")
    file(CREATE_LINK "${_SF}" "${_SYPHON_INC}/Syphon" SYMBOLIC)
endif()

# -----------------------------------------------------------------------------
# Syphon sources: Metal backend + shared messaging/IPC only. The OpenGL backend
# (GL/CGL renderers, legacy IOSurface image, the GL-based SyphonServer/Client)
# is intentionally excluded — TrussC renders with Metal.
# -----------------------------------------------------------------------------
set(_SYPHON_SRCS
    SyphonMetalServer.m
    SyphonMetalClient.m
    SyphonServerRendererMetal.m
    SyphonServerBase.m
    SyphonClientBase.m
    SyphonServerConnectionManager.m
    SyphonClientConnectionManager.m
    SyphonServerDirectory.m
    SyphonPrivate.m
    SyphonMessaging.m
    SyphonMessageSender.m
    SyphonMessageReceiver.m
    SyphonCFMessageSender.m
    SyphonCFMessageReceiver.m
    SyphonMessageQueue.m
    SyphonDispatch.c
)
# Note: the Syphon*Image / SyphonIOSurfaceImageCore sources are intentionally
# omitted — they are the OpenGL image path and pull in deprecated GL headers.
# The Metal client/server return id<MTLTexture> directly and never use them.
list(TRANSFORM _SYPHON_SRCS PREPEND "${_SF}/")

# Syphon-Framework is an ARC project (CLANG_ENABLE_OBJC_ARC = YES; some files
# use __weak) and relies on its prefix header for the Cocoa import + SYPHONLOG
# macro. The .c file ignores the ARC flag harmlessly.
set_source_files_properties(${_SYPHON_SRCS} PROPERTIES
    COMPILE_OPTIONS "-fobjc-arc;-include;${_SF}/Syphon_Prefix.pch")

# -----------------------------------------------------------------------------
# Compile Syphon's Metal shaders into default.metallib. SyphonMetalServer fails
# to initialise if its renderer can't load these, so the metallib must ship in
# the consuming app's bundle (handled below via tc_addon_bundle_file).
# -----------------------------------------------------------------------------
set(_METALLIB "${CMAKE_CURRENT_BINARY_DIR}/default.metallib")
set(_METAL_AIR "${CMAKE_CURRENT_BINARY_DIR}/SyphonMetalShaders.air")
add_custom_command(
    OUTPUT "${_METALLIB}"
    COMMAND xcrun -sdk macosx metal -c "${_SF}/SyphonMetalShaders.metal" -o "${_METAL_AIR}"
    COMMAND xcrun -sdk macosx metallib "${_METAL_AIR}" -o "${_METALLIB}"
    DEPENDS "${_SF}/SyphonMetalShaders.metal"
    COMMENT "[tcxSyphon] Compiling Syphon Metal shaders -> default.metallib"
    VERBATIM)
add_custom_target(tcxSyphon_metallib DEPENDS "${_METALLIB}")

# -----------------------------------------------------------------------------
# Addon library: our Objective-C++ wrappers (ARC) + the Syphon sources (non-ARC).
# -----------------------------------------------------------------------------
file(GLOB ADDON_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm")
file(GLOB ADDON_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
set_source_files_properties(${ADDON_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS} ${_SYPHON_SRCS})
add_dependencies(${ADDON_NAME} tcxSyphon_metallib)

target_include_directories(${ADDON_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(${ADDON_NAME} PRIVATE "${_SYPHON_INC}")

target_link_libraries(${ADDON_NAME} PUBLIC TrussC)
target_link_libraries(${ADDON_NAME} PUBLIC
    "-framework Metal"
    "-framework IOSurface"
    "-framework CoreVideo"
    "-framework Foundation"
    "-framework Cocoa"
    "-framework IOKit")

# Ship the Syphon metallib into the consuming app bundle's Resources, so
# SyphonMetalServer's renderer can load it at runtime via bundleForClass.
if(COMMAND tc_addon_bundle_file)
    tc_addon_bundle_file("${_METALLIB}" MACOS_DEST "Resources")
else()
    message(WARNING "[tcxSyphon] tc_addon_bundle_file not available — "
        "SyphonServer will fail unless default.metallib is bundled manually.")
endif()

add_library(tc::${ADDON_NAME} ALIAS ${ADDON_NAME})
