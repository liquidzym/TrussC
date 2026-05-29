# tcxAssimp - TrussC 3D Model Loading Addon (Assimp-based)
cmake_minimum_required(VERSION 3.16)
set(ADDON_NAME tcxAssimp)

# Share Assimp source/build directories across all projects using this addon.
# TrussC loads addons inside each app's build tree by default; without explicit
# FetchContent directories, every new app gets its own _deps/assimp-* clone and
# recompiles Assimp. Keep source platform-independent, and build outputs
# platform-specific to avoid incompatible object reuse.
if(EMSCRIPTEN)
    set(_TCX_ASSIMP_PLATFORM "web")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(_TCX_ASSIMP_PLATFORM "ios")
elseif(ANDROID)
    set(_TCX_ASSIMP_PLATFORM "android")
elseif(APPLE)
    set(_TCX_ASSIMP_PLATFORM "macos")
elseif(WIN32)
    set(_TCX_ASSIMP_PLATFORM "windows")
else()
    set(_TCX_ASSIMP_PLATFORM "linux")
endif()
set(_TCX_ASSIMP_CACHE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/_fcache")
set(_TCX_ASSIMP_BUILD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build-${_TCX_ASSIMP_PLATFORM}")

# Fetch Assimp
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "")
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "")
set(ASSIMP_INSTALL OFF CACHE BOOL "")
set(ASSIMP_NO_EXPORT ON CACHE BOOL "")
set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_COLLADA_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "")
if(APPLE AND NOT ASSIMP_BUILD_ZLIB)
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _TCX_MACOS_SDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_TCX_MACOS_SDK)
        set(ZLIB_INCLUDE_DIR "${_TCX_MACOS_SDK}/usr/include" CACHE PATH "" FORCE)
        set(ZLIB_LIBRARY_DBG "${_TCX_MACOS_SDK}/usr/lib/libz.tbd" CACHE FILEPATH "" FORCE)
        set(ZLIB_LIBRARY_REL "${_TCX_MACOS_SDK}/usr/lib/libz.tbd" CACHE FILEPATH "" FORCE)
    endif()
endif()
set(ASSIMP_ARCHIVE_OUTPUT_DIRECTORY "${_TCX_ASSIMP_BUILD_ROOT}/assimp-build/lib" CACHE STRING "" FORCE)
set(ASSIMP_LIBRARY_OUTPUT_DIRECTORY "${_TCX_ASSIMP_BUILD_ROOT}/assimp-build/bin" CACHE STRING "" FORCE)
set(ASSIMP_RUNTIME_OUTPUT_DIRECTORY "${_TCX_ASSIMP_BUILD_ROOT}/assimp-build/bin" CACHE STRING "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
    UPDATE_DISCONNECTED TRUE
    SOURCE_DIR "${_TCX_ASSIMP_CACHE_DIR}/assimp-src"
    BINARY_DIR "${_TCX_ASSIMP_BUILD_ROOT}/assimp-build"
    SUBBUILD_DIR "${_TCX_ASSIMP_BUILD_ROOT}/assimp-subbuild"
)
if(NOT TARGET assimp)
    FetchContent_MakeAvailable(assimp)
endif()

# Patch Assimp for Clang 21: remove :: prefix from all global-scope C function calls
file(READ "${assimp_SOURCE_DIR}/include/assimp/material.inl" _mat_inl)
set(_mat_inl_patched "${_mat_inl}")
string(REGEX REPLACE "::aiGet" "aiGet" _mat_inl_patched "${_mat_inl_patched}")
string(REGEX REPLACE "::aiReturn" "aiReturn" _mat_inl_patched "${_mat_inl_patched}")
string(REGEX REPLACE "::aiCreate" "aiCreate" _mat_inl_patched "${_mat_inl_patched}")
string(REGEX REPLACE "::aiRelease" "aiRelease" _mat_inl_patched "${_mat_inl_patched}")
if(NOT _mat_inl STREQUAL _mat_inl_patched)
    file(WRITE "${assimp_SOURCE_DIR}/include/assimp/material.inl" "${_mat_inl_patched}")
endif()
# Suppress Assimp -Werror=nontrivial-memcall on Clang 21
target_compile_options(assimp PRIVATE -w)

set(ADDON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Animation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GpuSkinnedRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Importer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/TextureResolver.cpp
)

set(ADDON_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcxAssimp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Animation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/AssimpConvert.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Bone.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/GpuSkinnedRenderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Importer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/Node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/SceneData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tcx/assimp/TextureResolver.h
)

set(_TCX_ASSIMP_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/tcxAssimpSkinned.glsl")
set(_TCX_ASSIMP_SHADER_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/tcxAssimpSkinned.glsl.h")
set(_TCX_SOKOL_SHDC "${CMAKE_CURRENT_SOURCE_DIR}/../../core/tools/sokol-shdc/sokol-shdc")
if(WIN32)
    set(_TCX_SOKOL_SHDC "${_TCX_SOKOL_SHDC}.exe")
endif()
if(EXISTS "${_TCX_SOKOL_SHDC}")
    add_custom_command(
        OUTPUT "${_TCX_ASSIMP_SHADER_HEADER}"
        COMMAND "${_TCX_SOKOL_SHDC}" -i "${_TCX_ASSIMP_SHADER}" -o "${_TCX_ASSIMP_SHADER_HEADER}" -l metal_macos:metal_ios:hlsl5:glsl300es:glsl430:wgsl --ifdef
        DEPENDS "${_TCX_ASSIMP_SHADER}"
        COMMENT "[tcxAssimp] Compiling shader: tcxAssimpSkinned.glsl"
    )
    add_custom_target(${ADDON_NAME}_shaders DEPENDS "${_TCX_ASSIMP_SHADER_HEADER}")
else()
    message(WARNING "[tcxAssimp] sokol-shdc not found; using checked-in shader header if present: ${_TCX_ASSIMP_SHADER_HEADER}")
endif()

add_library(${ADDON_NAME} STATIC ${ADDON_SOURCES} ${ADDON_HEADERS})
target_include_directories(${ADDON_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${ADDON_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(${ADDON_NAME} PUBLIC assimp TrussC)
if(TARGET ${ADDON_NAME}_shaders)
    add_dependencies(${ADDON_NAME} ${ADDON_NAME}_shaders)
endif()
