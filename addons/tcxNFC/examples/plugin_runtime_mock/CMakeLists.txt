cmake_minimum_required(VERSION 3.20)
project(tcxNFC_plugin_runtime_mock)

add_subdirectory("../.." "${CMAKE_BINARY_DIR}/tcxNFC")

add_executable(tcxNFC_plugin_runtime_mock main.cpp)
target_link_libraries(tcxNFC_plugin_runtime_mock PRIVATE tcxNFC)
target_compile_features(tcxNFC_plugin_runtime_mock PRIVATE cxx_std_20)
