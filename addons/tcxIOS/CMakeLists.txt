cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxIOS] Loading addon...")

option(TCX_IOS_ENABLE_CAMERA "Enable AVFoundation camera bridge" ON)
option(TCX_IOS_ENABLE_PHOTOS "Enable PhotosUI bridge" ON)
option(TCX_IOS_ENABLE_LOCATION "Enable CoreLocation bridge" ON)
option(TCX_IOS_ENABLE_BLUETOOTH "Enable CoreBluetooth bridge" ON)
option(TCX_IOS_ENABLE_SECURITY "Enable Security and LocalAuthentication bridge" ON)
option(TCX_IOS_BUILD_TESTS "Build tcxIOS tests from the addon target" OFF)

file(GLOB_RECURSE TCX_IOS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB_RECURSE TCX_IOS_COMMON_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp"
)

add_library(tcxIOS STATIC
    ${TCX_IOS_HEADERS}
    ${TCX_IOS_COMMON_SOURCES}
)

target_include_directories(tcxIOS PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(tcxIOS PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/private"
)

target_compile_features(tcxIOS PUBLIC cxx_std_20)

if(TARGET TrussC)
    target_link_libraries(tcxIOS PUBLIC TrussC)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR IOS)
    file(GLOB_RECURSE TCX_IOS_NATIVE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/*.mm"
    )
    target_sources(tcxIOS PRIVATE ${TCX_IOS_NATIVE_SOURCES})
    target_compile_options(tcxIOS PRIVATE "$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>")
    target_link_libraries(tcxIOS PRIVATE
        "-framework UIKit"
        "-framework Foundation"
        "-framework ImageIO"
        "-framework AVFoundation"
        "-framework AVFAudio"
        "-framework BackgroundTasks"
        "-framework ARKit"
        "-framework Contacts"
        "-framework ContactsUI"
        "-framework CoreBluetooth"
        "-framework CoreImage"
        "-framework CoreLocation"
        "-framework CoreML"
        "-framework CoreMedia"
        "-framework CoreVideo"
        "-framework GameController"
        "-framework LocalAuthentication"
        "-framework MultipeerConnectivity"
        "-framework Network"
        "-framework PencilKit"
        "-framework Photos"
        "-framework PhotosUI"
        "-framework SafariServices"
        "-framework Security"
        "-framework StoreKit"
        "-framework UniformTypeIdentifiers"
        "-framework Vision"
        "-framework CoreMotion"
        "-framework UserNotifications"
    )
else()
    file(GLOB_RECURSE TCX_IOS_STUB_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/stub/*.cpp"
    )
    target_sources(tcxIOS PRIVATE ${TCX_IOS_STUB_SOURCES})
endif()

if(TCX_IOS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

message(STATUS "[tcxIOS] Addon loaded")
