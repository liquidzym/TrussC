cmake_minimum_required(VERSION 3.20)
project(tcxClothTests)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core")
endif()

add_subdirectory("${TRUSSC_DIR}" "${CMAKE_BINARY_DIR}/trussc-core")
add_subdirectory(".." "${CMAKE_BINARY_DIR}/tcxCloth")

add_executable(tcxCloth_topology test_topology.cpp)
target_link_libraries(tcxCloth_topology PRIVATE tcxCloth)
target_compile_features(tcxCloth_topology PRIVATE cxx_std_20)

add_executable(tcxCloth_cpu_reference test_cpu_reference.cpp)
target_link_libraries(tcxCloth_cpu_reference PRIVATE tcxCloth)
target_compile_features(tcxCloth_cpu_reference PRIVATE cxx_std_20)

add_executable(tcxCloth_source_contracts test_source_contracts.cpp)
target_link_libraries(tcxCloth_source_contracts PRIVATE tcxCloth)
target_compile_features(tcxCloth_source_contracts PRIVATE cxx_std_20)
