cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxCloth] Loading addon...")

file(GLOB_RECURSE TCX_CLOTH_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_CLOTH_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE TCX_CLOTH_SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.glsl"
)

add_library(tcxCloth STATIC
    ${TCX_CLOTH_HEADERS}
    ${TCX_CLOTH_SOURCES}
)

target_include_directories(tcxCloth PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

target_compile_features(tcxCloth PUBLIC cxx_std_20)
target_link_libraries(tcxCloth PUBLIC TrussC)

if(TCX_CLOTH_SHADER_SOURCES)
    set(_TC_SOKOL_SHDC_BASE_URL "https://raw.githubusercontent.com/floooh/sokol-tools-bin/master/bin")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(_TC_SOKOL_SHDC_DIR "osx_arm64")
        else()
            set(_TC_SOKOL_SHDC_DIR "osx")
        endif()
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_TC_SOKOL_SHDC_DIR "win32")
    else()
        if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
            set(_TC_SOKOL_SHDC_DIR "linux_arm64")
        else()
            set(_TC_SOKOL_SHDC_DIR "linux")
        endif()
    endif()

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_TC_SOKOL_SHDC_EXT ".exe")
    else()
        set(_TC_SOKOL_SHDC_EXT "")
    endif()

    get_target_property(_TC_ROOT TrussC INTERFACE_INCLUDE_DIRECTORIES)
    list(GET _TC_ROOT 0 _TC_ROOT_DIR)
    get_filename_component(_TC_ROOT_DIR "${_TC_ROOT_DIR}" DIRECTORY)
    set(_TC_SOKOL_SHDC "${_TC_ROOT_DIR}/tools/sokol-shdc/sokol-shdc${_TC_SOKOL_SHDC_EXT}")
    set(_TC_SOKOL_SHDC_URL "${_TC_SOKOL_SHDC_BASE_URL}/${_TC_SOKOL_SHDC_DIR}/sokol-shdc${_TC_SOKOL_SHDC_EXT}")

    if(NOT EXISTS "${_TC_SOKOL_SHDC}")
        message(STATUS "[tcxCloth] Downloading sokol-shdc...")
        file(MAKE_DIRECTORY "${_TC_ROOT_DIR}/tools/sokol-shdc")
        file(DOWNLOAD "${_TC_SOKOL_SHDC_URL}" "${_TC_SOKOL_SHDC}"
            SHOW_PROGRESS
            STATUS _TC_DOWNLOAD_STATUS)
        list(GET _TC_DOWNLOAD_STATUS 0 _TC_DOWNLOAD_ERROR)
        if(_TC_DOWNLOAD_ERROR)
            message(FATAL_ERROR "Failed to download sokol-shdc: ${_TC_DOWNLOAD_STATUS}")
        endif()
        if(NOT CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            file(CHMOD "${_TC_SOKOL_SHDC}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
        endif()
    endif()

    set(_TC_SOKOL_SLANG "metal_macos:hlsl5:glsl410:glsl300es")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated")
    foreach(_SHADER_SRC ${TCX_CLOTH_SHADER_SOURCES})
        get_filename_component(_SHADER_NAME "${_SHADER_SRC}" NAME)
        set(_SHADER_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated/${_SHADER_NAME}.h")
        add_custom_command(
            OUTPUT "${_SHADER_OUTPUT}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/generated"
            COMMAND "${_TC_SOKOL_SHDC}"
                --input "${_SHADER_SRC}"
                --output "${_SHADER_OUTPUT}"
                --slang ${_TC_SOKOL_SLANG}
                --format sokol
            DEPENDS "${_SHADER_SRC}"
            COMMENT "[tcxCloth] Compiling shader: ${_SHADER_SRC}"
        )
        list(APPEND TCX_CLOTH_SHADER_OUTPUTS "${_SHADER_OUTPUT}")
    endforeach()

    add_custom_target(tcxCloth_shaders DEPENDS ${TCX_CLOTH_SHADER_OUTPUTS})
    add_dependencies(tcxCloth tcxCloth_shaders)
endif()

message(STATUS "[tcxCloth] Addon loaded")
