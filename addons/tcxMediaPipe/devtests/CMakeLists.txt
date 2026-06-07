cmake_minimum_required(VERSION 3.20)
project(tcxMediaPipe_devtests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TCX_MEDIAPIPE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(TRUSSC_CORE_DIR "${TCX_MEDIAPIPE_ROOT}/../../core")

add_executable(tcxMediaPipe_parser_tests
    parser_test.cpp
    "${TCX_MEDIAPIPE_ROOT}/src/tcxMediaPipeResultParser.cpp"
)

target_include_directories(tcxMediaPipe_parser_tests PRIVATE
    "${TCX_MEDIAPIPE_ROOT}/src"
    "${TRUSSC_CORE_DIR}/include"
)
