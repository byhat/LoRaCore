/**
 * @file main.cpp
 * @brief gtest main entry point for LoRaCore unit tests
 * @date 2026-02-01
 */

#include <gtest/gtest.h>

/**
 * @brief Main entry point for the test suite
 * @param argc Argument count
 * @param argv Argument values
 * @return Test result code (0 for success, non-zero for failures)
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
