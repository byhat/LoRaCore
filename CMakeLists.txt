cmake_minimum_required(VERSION 3.19)
project(LoRaCore LANGUAGES CXX)

# Deliberately distinct from the parent project's BUILD_TESTS so the app's
# test switch does not enable these (currently stale) tests.
option(LORACORE_BUILD_TESTS "BUILD LoRaCore UNIT TESTS" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(Qt6 REQUIRED COMPONENTS Core)
if(LORACORE_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
endif()

if(LORACORE_BUILD_TESTS)
    # Fetch Google Test
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )

    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Add CrossSerial subdirectory (static, no examples/tests of its own here)
set(CROSSSERIAL_BUILD_SHARED  OFF CACHE BOOL "" FORCE)
set(CROSSSERIAL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROSSSERIAL_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/CrossSerial)

qt_add_library(LoRaCore STATIC
    src/LoRaWorker.hpp
    src/LoRaWorker.cpp
    src/LoRaUsbAdapter_E22_400T22U.cpp
    src/LoRaUsbAdapter_E22_400T22U.hpp
    src/ProtocolStructures.hpp
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
        crossserial
)

if(LORACORE_BUILD_TESTS)
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
            Qt6::Test
            crossserial
            LoRaCore
    )

    set_target_properties(LoRaCoreTests PROPERTIES AUTOMOC ON)

    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(LoRaCoreTests)
endif()
