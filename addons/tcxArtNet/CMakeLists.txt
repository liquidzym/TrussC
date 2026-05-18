cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxArtNet] Loading addon...")

file(GLOB_RECURSE TCX_ARTNET_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_ARTNET_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(tcxArtNet STATIC
    ${TCX_ARTNET_HEADERS}
    ${TCX_ARTNET_SOURCES}
)

target_include_directories(tcxArtNet PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(tcxArtNet PUBLIC cxx_std_20)

if(TARGET TrussC)
    target_link_libraries(tcxArtNet PUBLIC TrussC)
endif()

if(WIN32)
    target_link_libraries(tcxArtNet PUBLIC ws2_32)
endif()

message(STATUS "[tcxArtNet] Addon loaded")
