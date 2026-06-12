cmake_minimum_required(VERSION 3.20)
project(tcxCEF LANGUAGES C CXX)

set(ADDON_NAME tcxCEF)

option(TCXCEF_ADHOC_SIGN_APP_BUNDLES
    "Ad-hoc sign macOS app bundles after copying CEF runtime files for local privacy services."
    ON)

file(GLOB_RECURSE TCX_CEF_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_CEF_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(APPLE)
    list(APPEND TCX_CEF_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/tcxcef/MacApplication.mm"
    )
endif()

add_library(${ADDON_NAME} STATIC
    ${TCX_CEF_HEADERS}
    ${TCX_CEF_SOURCES}
)
add_library(tc::tcxCEF ALIAS ${ADDON_NAME})

target_include_directories(${ADDON_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(${ADDON_NAME} PUBLIC cxx_std_20)
target_compile_definitions(${ADDON_NAME} PUBLIC
    TCXCEF_ADDON_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

if(TARGET TrussC)
    target_link_libraries(${ADDON_NAME} PUBLIC TrussC)
else()
    set(_TCXCEF_TRUSSC_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../../core")
    target_include_directories(${ADDON_NAME} PUBLIC "${_TCXCEF_TRUSSC_CORE}/include")
    target_sources(${ADDON_NAME} PRIVATE
        "${_TCXCEF_TRUSSC_CORE}/include/tc/network/tcTcpServer.cpp"
    )
endif()

if(TARGET TrussC)
    if(NOT TARGET tcxTls)
        set(_TCXCEF_TLS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../tcxTls")
        if(EXISTS "${_TCXCEF_TLS_PATH}/CMakeLists.txt")
            add_subdirectory("${_TCXCEF_TLS_PATH}" "${CMAKE_BINARY_DIR}/addons/tcxTls")
            list(APPEND _TC_LOADED_ADDONS tcxTls)
            set(_TC_LOADED_ADDONS "${_TC_LOADED_ADDONS}" CACHE INTERNAL "List of loaded TrussC addons")
        endif()
    endif()

    if(NOT TARGET tcxWebSocket)
        set(_TCXCEF_WEBSOCKET_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../tcxWebSocket")
        if(EXISTS "${_TCXCEF_WEBSOCKET_PATH}/CMakeLists.txt")
            add_subdirectory("${_TCXCEF_WEBSOCKET_PATH}" "${CMAKE_BINARY_DIR}/addons/tcxWebSocket")
            list(APPEND _TC_LOADED_ADDONS tcxWebSocket)
            set(_TC_LOADED_ADDONS "${_TC_LOADED_ADDONS}" CACHE INTERNAL "List of loaded TrussC addons")
        endif()
    endif()
    if(TARGET tcxWebSocket)
        target_link_libraries(${ADDON_NAME} PUBLIC tcxWebSocket)
    endif()
endif()

set(TCXCEF_PATHS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/libs/cef/current/cef_paths.cmake")
if(EXISTS "${TCXCEF_PATHS_FILE}")
    include("${TCXCEF_PATHS_FILE}")
    set(TCXCEF_HAS_CEF_VALUE 1)
else()
    set(TCXCEF_HAS_CEF_VALUE 0)
    message(WARNING
        "[tcxCEF] CEF runtime is not setup. Run:\n"
        "  python ${CMAKE_CURRENT_SOURCE_DIR}/tools/setup_cef.py --config Release")
endif()
set(TCXCEF_HAS_CEF_VALUE "${TCXCEF_HAS_CEF_VALUE}" CACHE INTERNAL "tcxCEF runtime availability")

if(TCXCEF_HAS_CEF_VALUE)
    foreach(_TCXCEF_CACHE_VAR IN ITEMS
        TCXCEF_PLATFORM
        TCXCEF_VERSION
        TCXCEF_ROOT
        TCXCEF_SOURCE_DIR
        TCXCEF_INCLUDE_DIR
        TCXCEF_RELEASE_DIR
        TCXCEF_RESOURCE_DIR
        TCXCEF_LIBCEF_LIBRARY
        TCXCEF_CEF_FRAMEWORK_PATH
        TCXCEF_WRAPPER_LIBRARY
        TCXCEF_RUNTIME_FILES
        TCXCEF_RESOURCE_FILES
    )
        set(${_TCXCEF_CACHE_VAR} "${${_TCXCEF_CACHE_VAR}}" CACHE INTERNAL "tcxCEF generated CEF path")
    endforeach()
endif()

target_compile_definitions(${ADDON_NAME} PUBLIC TCXCEF_HAS_CEF=${TCXCEF_HAS_CEF_VALUE})

if(TCXCEF_HAS_CEF_VALUE)
    target_compile_definitions(${ADDON_NAME} PUBLIC
        TCXCEF_PLATFORM_NAME="${TCXCEF_PLATFORM}"
        TCXCEF_VERSION_STRING="${TCXCEF_VERSION}"
        TCXCEF_ROOT_PATH="${TCXCEF_ROOT}"
        TCXCEF_INCLUDE_PATH="${TCXCEF_INCLUDE_DIR}"
        TCXCEF_RELEASE_PATH="${TCXCEF_RELEASE_DIR}"
        TCXCEF_RESOURCE_PATH="${TCXCEF_RESOURCE_DIR}"
        TCXCEF_LIBCEF_PATH="${TCXCEF_LIBCEF_LIBRARY}"
        TCXCEF_FRAMEWORK_PATH="${TCXCEF_CEF_FRAMEWORK_PATH}"
        TCXCEF_WRAPPER_PATH="${TCXCEF_WRAPPER_LIBRARY}"
    )
    target_include_directories(${ADDON_NAME} PRIVATE "${TCXCEF_INCLUDE_DIR}")
    target_link_libraries(${ADDON_NAME} PRIVATE "${TCXCEF_WRAPPER_LIBRARY}")

    if(APPLE)
        target_link_libraries(${ADDON_NAME} PRIVATE "${TCXCEF_CEF_FRAMEWORK_PATH}")
    elseif(WIN32)
        target_link_libraries(${ADDON_NAME} PRIVATE "${TCXCEF_LIBCEF_LIBRARY}")
        target_link_libraries(${ADDON_NAME} PRIVATE user32 shell32 ole32 gdi32 shlwapi)
    elseif(UNIX)
        target_link_libraries(${ADDON_NAME} PRIVATE "${TCXCEF_LIBCEF_LIBRARY}" pthread dl)
        target_link_options(${ADDON_NAME} PUBLIC "-Wl,-rpath,$ORIGIN")
    endif()
endif()

function(tcxcef_add_macos_helpers TARGET_NAME)
    if(NOT APPLE OR NOT TCXCEF_HAS_CEF_VALUE)
        return()
    endif()

    set(_TCXCEF_HELPER_SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/tcxcef/ProcessHelperMac.cc")
    set(_TCXCEF_HELPER_SUFFIXES
        "::"
        " (Alerts):_alerts:.alerts"
        " (GPU):_gpu:.gpu"
        " (Plugin):_plugin:.plugin"
        " (Renderer):_renderer:.renderer"
    )

    foreach(_TCXCEF_SUFFIX_LIST IN LISTS _TCXCEF_HELPER_SUFFIXES)
        string(REPLACE ":" ";" _TCXCEF_SUFFIX_PARTS "${_TCXCEF_SUFFIX_LIST}")
        list(GET _TCXCEF_SUFFIX_PARTS 0 _TCXCEF_NAME_SUFFIX)
        list(GET _TCXCEF_SUFFIX_PARTS 1 _TCXCEF_TARGET_SUFFIX)
        list(GET _TCXCEF_SUFFIX_PARTS 2 _TCXCEF_PLIST_SUFFIX)

        set(_TCXCEF_HELPER_OUTPUT_NAME "${TARGET_NAME} Helper${_TCXCEF_NAME_SUFFIX}")
        string(MAKE_C_IDENTIFIER
            "${TARGET_NAME}_tcxCEF_Helper${_TCXCEF_TARGET_SUFFIX}"
            _TCXCEF_HELPER_TARGET)
        set(_TCXCEF_HELPER_PLIST
            "${CMAKE_CURRENT_BINARY_DIR}/${_TCXCEF_HELPER_TARGET}-Info.plist")
        set(_TCXCEF_HELPER_BUNDLE_ID
            "com.trussc.${TARGET_NAME}.helper${_TCXCEF_PLIST_SUFFIX}")

        file(WRITE "${_TCXCEF_HELPER_PLIST}" "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleDisplayName</key>
    <string>${_TCXCEF_HELPER_OUTPUT_NAME}</string>
    <key>CFBundleExecutable</key>
    <string>${_TCXCEF_HELPER_OUTPUT_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>${_TCXCEF_HELPER_BUNDLE_ID}</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>${_TCXCEF_HELPER_OUTPUT_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>LSEnvironment</key>
    <dict>
        <key>MallocNanoZone</key>
        <string>0</string>
    </dict>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
    <key>LSUIElement</key>
    <string>1</string>
    <key>NSCameraUsageDescription</key>
    <string>This app uses the camera for CEF video capture.</string>
    <key>NSCameraUseContinuityCameraDeviceType</key>
    <true/>
    <key>NSMicrophoneUsageDescription</key>
    <string>This app can use the microphone for CEF media capture.</string>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
")

        add_executable(${_TCXCEF_HELPER_TARGET}
            MACOSX_BUNDLE
            EXCLUDE_FROM_ALL
            "${_TCXCEF_HELPER_SOURCE}"
        )
        target_compile_features(${_TCXCEF_HELPER_TARGET} PRIVATE cxx_std_20)
        target_include_directories(${_TCXCEF_HELPER_TARGET} PRIVATE "${TCXCEF_INCLUDE_DIR}")
        target_link_libraries(${_TCXCEF_HELPER_TARGET} PRIVATE
            "${TCXCEF_WRAPPER_LIBRARY}"
            "-framework AppKit"
            "-framework Cocoa"
            "-framework IOSurface"
            "-lpthread"
        )
        set_target_properties(${_TCXCEF_HELPER_TARGET} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${_TCXCEF_HELPER_PLIST}"
            OUTPUT_NAME "${_TCXCEF_HELPER_OUTPUT_NAME}"
        )

        add_dependencies(${TARGET_NAME} ${_TCXCEF_HELPER_TARGET})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<TARGET_BUNDLE_DIR:${_TCXCEF_HELPER_TARGET}>"
                "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Frameworks/${_TCXCEF_HELPER_OUTPUT_NAME}.app"
            COMMENT "[tcxCEF] Copying ${_TCXCEF_HELPER_OUTPUT_NAME}.app"
            VERBATIM)
    endforeach()
endfunction()

function(tcxcef_copy_runtime_files TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "[tcxCEF] tcxcef_copy_runtime_files target not found: ${TARGET_NAME}")
    endif()
    if(NOT TCXCEF_HAS_CEF_VALUE)
        set(_TCXCEF_SETUP_SCRIPT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tools/setup_cef.py")
        message(FATAL_ERROR
            "[tcxCEF] CEF runtime is missing. Run:\n"
            "  python ${_TCXCEF_SETUP_SCRIPT} --config Release")
    endif()

    if(APPLE)
        tcxcef_add_macos_helpers(${TARGET_NAME})
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${TCXCEF_CEF_FRAMEWORK_PATH}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Frameworks/Chromium Embedded Framework.framework"
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Resources"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${TCXCEF_RESOURCE_DIR}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Resources"
            COMMENT "[tcxCEF] Copying CEF framework and resources"
            VERBATIM)
        if(TCXCEF_ADHOC_SIGN_APP_BUNDLES)
            find_program(TCXCEF_CODESIGN_EXECUTABLE codesign)
            if(TCXCEF_CODESIGN_EXECUTABLE)
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND "${TCXCEF_CODESIGN_EXECUTABLE}" --force --deep --sign -
                        "$<TARGET_BUNDLE_DIR:${TARGET_NAME}>"
                    COMMENT "[tcxCEF] Ad-hoc signing ${TARGET_NAME}.app for macOS privacy services"
                    VERBATIM)
            else()
                message(WARNING "[tcxCEF] codesign not found; macOS camera permissions may not work for ${TARGET_NAME}.app")
            endif()
        endif()
    else()
        foreach(_TCXCEF_RUNTIME_FILE IN LISTS TCXCEF_RUNTIME_FILES TCXCEF_RESOURCE_FILES)
            if(EXISTS "${_TCXCEF_RUNTIME_FILE}")
                add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_TCXCEF_RUNTIME_FILE}"
                        "$<TARGET_FILE_DIR:${TARGET_NAME}>"
                    COMMENT "[tcxCEF] Copying ${_TCXCEF_RUNTIME_FILE}"
                    VERBATIM)
            endif()
        endforeach()
        if(EXISTS "${TCXCEF_RESOURCE_DIR}/locales")
            add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${TCXCEF_RESOURCE_DIR}/locales"
                    "$<TARGET_FILE_DIR:${TARGET_NAME}>/locales"
                COMMENT "[tcxCEF] Copying CEF locales"
                VERBATIM)
        endif()
    endif()
endfunction()
