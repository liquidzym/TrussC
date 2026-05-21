cmake_minimum_required(VERSION 3.20)
project(tcxArtNetTools)

set(TRUSSC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../core")

add_subdirectory("${TRUSSC_DIR}" "${CMAKE_BINARY_DIR}/trussc-core")
add_subdirectory("../.." "${CMAKE_BINARY_DIR}/tcxArtNet")

add_executable(artnet_socket_probe artnet_socket_probe.cpp)
target_link_libraries(artnet_socket_probe PRIVATE tcxArtNet)
target_compile_features(artnet_socket_probe PRIVATE cxx_std_20)

add_executable(artnet_output_probe artnet_output_probe.cpp)
target_link_libraries(artnet_output_probe PRIVATE tcxArtNet)
target_compile_features(artnet_output_probe PRIVATE cxx_std_20)
