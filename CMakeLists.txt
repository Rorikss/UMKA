cmake_minimum_required(VERSION 3.14)
project(UMKA-VM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(umka_vm
    UMKA-VM/main.cpp
    UMKA-VM/model/model.h
    UMKA-VM/parser/command_parser.h
    UMKA-VM/parser/command_parser.cpp
    UMKA-VM/runtime/operations.h
    UMKA-VM/runtime/stack_machine.h
    UMKA-VM/runtime/profiler.h
)

target_include_directories(umka_vm PRIVATE UMKA-VM)

# Google Test setup using FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Test executable
add_executable(stack_machine_tests
    UMKA-VM/tests/stack_machine_test.cpp
)
target_include_directories(stack_machine_tests PRIVATE UMKA-VM)
target_link_libraries(stack_machine_tests PRIVATE gtest_main)
enable_testing()
add_test(NAME stack_machine_tests COMMAND stack_machine_tests)