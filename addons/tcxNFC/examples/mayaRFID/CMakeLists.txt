cmake_minimum_required(VERSION 3.20)

project(mayaRFID VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

option(MAYARFID_BUILD_CLI "Build mayaRFID_cli diagnostics" ON)
option(MAYARFID_BUILD_GUI "Build mayaRFID TrussC GUI" ON)

set(_TCXNFC_CANDIDATES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendor/tcxNFC"
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

foreach(_candidate IN LISTS _TCXNFC_CANDIDATES)
    if(EXISTS "${_candidate}/CMakeLists.txt")
        get_filename_component(TCXNFC_SOURCE_DIR "${_candidate}" ABSOLUTE)
        break()
    endif()
endforeach()

if(NOT DEFINED TCXNFC_SOURCE_DIR)
    message(FATAL_ERROR "tcxNFC addon source was not found. Expected ../.. or vendor/tcxNFC.")
endif()

message(STATUS "[mayaRFID example] TCXNFC_SOURCE_DIR=${TCXNFC_SOURCE_DIR}")

if(MAYARFID_BUILD_GUI AND NOT TARGET TrussC)
    set(_TRUSSC_CANDIDATES
        "$ENV{TRUSSC_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/TrussC"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
        "$ENV{HOME}/Desktop/TrussC"
    )

    foreach(_candidate IN LISTS _TRUSSC_CANDIDATES)
        if(EXISTS "${_candidate}/core/CMakeLists.txt")
            get_filename_component(TRUSSC_SOURCE_DIR "${_candidate}" ABSOLUTE)
            break()
        endif()
    endforeach()

    if(DEFINED TRUSSC_SOURCE_DIR)
        message(STATUS "[mayaRFID example] TRUSSC_SOURCE_DIR=${TRUSSC_SOURCE_DIR}")
        add_subdirectory("${TRUSSC_SOURCE_DIR}/core" "${CMAKE_BINARY_DIR}/TrussC-core")
    else()
        message(WARNING "TrussC source was not found; mayaRFID_gui will not be built.")
    endif()
endif()

add_subdirectory("${TCXNFC_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/tcxNFC")

set(MAYARFID_RUNTIME_SOURCES
    src/common/Config.cpp
    src/common/Formatting.cpp
    src/runtime/ActivationRuntime.cpp
    src/cli/Commands.cpp
)

add_library(mayaRFID_runtime
    ${MAYARFID_RUNTIME_SOURCES}
)

target_include_directories(mayaRFID_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(mayaRFID_runtime PUBLIC
    MAYARFID_EXAMPLE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(mayaRFID_runtime PUBLIC tcxNFC)
target_compile_features(mayaRFID_runtime PUBLIC cxx_std_20)

add_executable(mayaRFID
    src/main.cpp
)
target_link_libraries(mayaRFID PRIVATE mayaRFID_runtime)

if(MAYARFID_BUILD_CLI)
    add_executable(mayaRFID_cli
        tools/mayaRFID_cli.cpp
    )
    target_link_libraries(mayaRFID_cli PRIVATE mayaRFID_runtime)
endif()

if(MAYARFID_BUILD_GUI AND TARGET TrussC)
    add_executable(mayaRFID_gui MACOSX_BUNDLE
        src/gui_main.cpp
        src/gui/MayaRFIDGuiApp.cpp
    )
    target_link_libraries(mayaRFID_gui PRIVATE mayaRFID_runtime)
    target_include_directories(mayaRFID_gui PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
endif()
