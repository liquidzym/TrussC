cmake_minimum_required(VERSION 3.20)
project(tcxArtNetTests)

if(NOT DEFINED TRUSSC_DIR)
    set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../core")
endif()

add_subdirectory("${TRUSSC_DIR}" "${CMAKE_BINARY_DIR}/trussc-core")
add_subdirectory(".." "${CMAKE_BINARY_DIR}/tcxArtNet")

file(GLOB TCX_ARTNET_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp"
)

add_executable(tcxArtNet_tests ${TCX_ARTNET_TEST_SOURCES})
target_link_libraries(tcxArtNet_tests PRIVATE tcxArtNet)
target_compile_features(tcxArtNet_tests PRIVATE cxx_std_20)
