cmake_minimum_required(VERSION 3.20)
project(tcxMediaPipe LANGUAGES C CXX)

set(ADDON_NAME tcxMediaPipe)

set(TCXMEDIAPIPE_CEF_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../tcxCEF/libs/cef/current/cef_paths.cmake")
set(TCXMEDIAPIPE_CEF_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/../tcxCEF/libs/cef/current/build_manifest.json")
if(NOT EXISTS "${TCXMEDIAPIPE_CEF_PATHS}" OR NOT EXISTS "${TCXMEDIAPIPE_CEF_MANIFEST}")
    message(FATAL_ERROR
        "[tcxMediaPipe] tcxCEF runtime is not setup.\n"
        "Run this first from the TrussC repo root:\n"
        "  python addons/tcxCEF/tools/setup_cef.py --config Release\n"
        "Expected:\n"
        "  ${TCXMEDIAPIPE_CEF_PATHS}\n"
        "  ${TCXMEDIAPIPE_CEF_MANIFEST}")
endif()

file(GLOB_RECURSE TCX_MEDIAPIPE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_MEDIAPIPE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${ADDON_NAME} STATIC
    ${TCX_MEDIAPIPE_HEADERS}
    ${TCX_MEDIAPIPE_SOURCES}
)
add_library(tc::tcxMediaPipe ALIAS ${ADDON_NAME})

target_include_directories(${ADDON_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_20)
target_compile_definitions(${ADDON_NAME} PUBLIC
    TCXMEDIAPIPE_ADDON_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(TARGET TrussC)
    target_link_libraries(${ADDON_NAME} PUBLIC TrussC)
endif()

if(NOT TARGET tcxCEF)
    set(_TCXMEDIAPIPE_CEF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../tcxCEF")
    if(EXISTS "${_TCXMEDIAPIPE_CEF_PATH}/CMakeLists.txt")
        add_subdirectory("${_TCXMEDIAPIPE_CEF_PATH}" "${CMAKE_BINARY_DIR}/addons/tcxCEF")
        list(APPEND _TC_LOADED_ADDONS tcxCEF)
        set(_TC_LOADED_ADDONS "${_TC_LOADED_ADDONS}" CACHE INTERNAL "List of loaded TrussC addons")
    else()
        message(FATAL_ERROR "[tcxMediaPipe] Required addon missing: tcxCEF")
    endif()
endif()

target_link_libraries(${ADDON_NAME} PUBLIC tcxCEF)

function(tcxmediapipe_copy_web_assets TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "[tcxMediaPipe] tcxmediapipe_copy_web_assets target not found: ${TARGET_NAME}")
    endif()

    set(_TCXMEDIAPIPE_WEB_ROOT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/web")
    if(NOT EXISTS "${_TCXMEDIAPIPE_WEB_ROOT}/dist/index.html")
        message(FATAL_ERROR
            "[tcxMediaPipe] web assets are missing. Run:\n"
            "  python ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/build_web_assets.py")
    endif()

    if(APPLE)
        set(_TCXMEDIAPIPE_WEB_DEST
            "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Resources/tcxMediaPipe/web")
    else()
        set(_TCXMEDIAPIPE_WEB_DEST
            "$<TARGET_FILE_DIR:${TARGET_NAME}>/tcxMediaPipe/web")
    endif()

    string(MAKE_C_IDENTIFIER
        "${TARGET_NAME}_tcxMediaPipe_web_assets"
        _TCXMEDIAPIPE_WEB_ASSETS_TARGET)

    set(_TCXMEDIAPIPE_CODESIGN_COMMAND)
    if(APPLE)
        if(NOT DEFINED TCXCEF_ADHOC_SIGN_APP_BUNDLES OR TCXCEF_ADHOC_SIGN_APP_BUNDLES)
            find_program(_TCXMEDIAPIPE_CODESIGN_EXECUTABLE codesign)
            if(_TCXMEDIAPIPE_CODESIGN_EXECUTABLE)
                list(APPEND _TCXMEDIAPIPE_CODESIGN_COMMAND
                    COMMAND "${_TCXMEDIAPIPE_CODESIGN_EXECUTABLE}" --force --deep --sign -
                        "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>")
            else()
                message(WARNING "[tcxMediaPipe] codesign not found; macOS camera permissions may not work for ${TARGET_NAME}.app")
            endif()
        endif()
    endif()

    add_custom_target(${_TCXMEDIAPIPE_WEB_ASSETS_TARGET} ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${_TCXMEDIAPIPE_WEB_DEST}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_TCXMEDIAPIPE_WEB_DEST}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_TCXMEDIAPIPE_WEB_ROOT}/dist"
            "${_TCXMEDIAPIPE_WEB_DEST}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_TCXMEDIAPIPE_WEB_ROOT}/models"
            "${_TCXMEDIAPIPE_WEB_DEST}/models"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${_TCXMEDIAPIPE_WEB_ROOT}/wasm"
            "${_TCXMEDIAPIPE_WEB_DEST}/wasm"
        ${_TCXMEDIAPIPE_CODESIGN_COMMAND}
        COMMENT "[tcxMediaPipe] Copying web runtime assets"
        VERBATIM)
    add_dependencies(${_TCXMEDIAPIPE_WEB_ASSETS_TARGET} ${TARGET_NAME})
endfunction()
