cmake_minimum_required(VERSION 3.20)
project(tcxNFC LANGUAGES CXX)

message(STATUS "[tcxNFC] Loading addon...")

file(GLOB_RECURSE TCX_NFC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_NFC_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(tcxNFC STATIC
    ${TCX_NFC_HEADERS}
    ${TCX_NFC_SOURCES}
)

target_include_directories(tcxNFC PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(tcxNFC PUBLIC cxx_std_20)

if(TARGET TrussC)
    target_link_libraries(tcxNFC PUBLIC TrussC)
endif()

if(WIN32)
    target_link_libraries(tcxNFC PUBLIC ws2_32)
endif()

message(STATUS "[tcxNFC] Addon loaded")
