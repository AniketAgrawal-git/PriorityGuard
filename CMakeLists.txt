cmake_minimum_required(VERSION 3.12)
project(PriorityGuard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/third_party
)

# Engine Executable
add_executable(priorityguard_engine
    engine/src/main.cpp
)

# Engine Test Suite
add_executable(priorityguard_tests
    engine/tests/test_runner.cpp
)

enable_testing()
add_test(NAME CoreEngineTests COMMAND priorityguard_tests)

