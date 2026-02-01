cmake_minimum_required(VERSION 3.19)
project(LoRaCore LANGUAGES CXX)

include(FetchContent)

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort Test)

# Fetch Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

qt_add_library(LoRaCore STATIC
    src/LoRaUsbAdapter_E22_400T22U.hpp
    src/LoRaUsbAdapter_E22_400T22U.cpp
    src/LoRaWorker.hpp
    src/LoRaWorker.cpp
)

add_library(LoRaCore::LoRaCore ALIAS LoRaCore)

target_compile_features(LoRaCore PRIVATE cxx_std_17)

target_include_directories(LoRaCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

set_target_properties(LoRaCore PROPERTIES AUTOMOC ON)

target_link_libraries(LoRaCore
    PUBLIC
        Qt6::Core
        Qt6::SerialPort
)

# Enable testing
enable_testing()

# Create test executable
qt_add_executable(LoRaCoreTests
    tests/main.cpp
    tests/LoRaUsbAdapterTests.cpp
    tests/LoRaWorkerTests.cpp
)

target_link_libraries(LoRaCoreTests
    PRIVATE
        gtest
        gtest_main
        Qt6::Core
        Qt6::SerialPort
        Qt6::Test
        LoRaCore
)

set_target_properties(LoRaCoreTests PROPERTIES AUTOMOC ON)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(LoRaCoreTests)
