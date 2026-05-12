cmake_minimum_required(VERSION 3.20)
project(tcxFlowToolsTests)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core")
endif()

add_subdirectory("${TRUSSC_DIR}" "${CMAKE_BINARY_DIR}/trussc-core")
add_subdirectory(".." "${CMAKE_BINARY_DIR}/tcxFlowTools")

add_executable(tcxFlowTools_settings test_settings.cpp)
target_link_libraries(tcxFlowTools_settings PRIVATE tcxFlowTools)

add_executable(tcxFlowTools_core_contracts test_core_contracts.cpp)
target_link_libraries(tcxFlowTools_core_contracts PRIVATE tcxFlowTools)
