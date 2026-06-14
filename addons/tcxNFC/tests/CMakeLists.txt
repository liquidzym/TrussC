cmake_minimum_required(VERSION 3.20)
project(tcxNFCTests)

add_subdirectory(".." "${CMAKE_BINARY_DIR}/tcxNFC")

add_executable(tcxNFC_tests
    test_runner.cpp
    test_ndef.cpp
    test_bks710i_frames.cpp
    test_activation_runtime.cpp
)

target_include_directories(tcxNFC_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_link_libraries(tcxNFC_tests PRIVATE tcxNFC)
target_compile_features(tcxNFC_tests PRIVATE cxx_std_20)
