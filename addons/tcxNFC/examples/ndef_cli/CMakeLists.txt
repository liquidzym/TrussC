cmake_minimum_required(VERSION 3.20)
project(tcxNFC_ndef_cli)

add_subdirectory("../.." "${CMAKE_BINARY_DIR}/tcxNFC")

add_executable(tcxNFC_ndef_cli main.cpp)
target_link_libraries(tcxNFC_ndef_cli PRIVATE tcxNFC)
target_compile_features(tcxNFC_ndef_cli PRIVATE cxx_std_20)
