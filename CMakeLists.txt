cmake_minimum_required(VERSION 3.19)
project(LoRaCore LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Core SerialPort)

qt_add_library(LoRaCore STATIC
    src/LoRaUsbAdapter_E22_400T22U.hpp
    src/LoRaUsbAdapter_E22_400T22U.cpp
    src/LoRaWorker.hpp
    src/LoRaWorker.cpp
)

target_compile_features(LoRaCore PRIVATE cxx_std_17)

target_include_directories(LoRaCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

set_target_properties(LoRaCore PROPERTIES AUTOMOC ON)

target_link_libraries(LoRaCore
    PUBLIC
        Qt6::Core
        Qt6::SerialPort
)
