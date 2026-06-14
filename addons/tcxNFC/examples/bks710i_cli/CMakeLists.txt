cmake_minimum_required(VERSION 3.20)
project(tcxNFC_bks710i_cli)

add_subdirectory("../.." "${CMAKE_BINARY_DIR}/tcxNFC")

add_executable(tcxNFC_bks710i_cli main.cpp)
target_link_libraries(tcxNFC_bks710i_cli PRIVATE tcxNFC)
target_compile_features(tcxNFC_bks710i_cli PRIVATE cxx_std_20)
