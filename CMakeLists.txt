cmake_minimum_required(VERSION 3.19)
project(LoRaCore LANGUAGES CXX)

option(BUILD_TESTS "BUILD UNIT TESTS" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

find_package(Qt6 REQUIRED COMPONENTS Core Test SerialPort StateMachine)

if(BUILD_TESTS)
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

# Add QCrossPlatformSerialPort subdirectory
add_subdirectory(thirdparty/QCrossPlatformSerial)

qt_add_library(LoRaCore STATIC
    # src/LoRaUsbAdapter_E22_400T22U.hpp
    # src/LoRaUsbAdapter_E22_400T22U.cpp
    src/LoRaWorker.hpp
    src/LoRaWorker.cpp
    src/LoRaUsbFastAdapter_E22_400T22U.hpp
    src/LoRaUsbFastAdapter_E22_400T22U.cpp
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
        Qt6::StateMachine
        QCrossPlatformSerial
)

if(BUILD_TESTS)
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
            Qt6::StateMachine
            Qt6::Test
            QCrossPlatformSerial
            LoRaCore
    )

    set_target_properties(LoRaCoreTests PROPERTIES AUTOMOC ON)

    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(LoRaCoreTests)
endif()
