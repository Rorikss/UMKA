cmake_minimum_required(VERSION 3.16)
project(UMKA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(CMAKE_BUILD_TYPE "Debug")

if(APPLE)
    find_program(HOMEBREW_BISON bison HINTS /opt/homebrew/opt/bison/bin /usr/local/opt/bison/bin)

    if(HOMEBREW_BISON)
        message(STATUS "Using Homebrew bison: ${HOMEBREW_BISON}")
        set(BISON_EXECUTABLE ${HOMEBREW_BISON})
    else()
        message(WARNING "System bison on macOS is too old (<2.7). Install Homebrew bison: brew install bison")
    endif()
endif()

set(PARSER_DIR ${CMAKE_SOURCE_DIR}/UMKA-LP)
set(COMPILER_DIR ${CMAKE_SOURCE_DIR}/UMKA-C)
set(COMPILER_MODELS_DIR ${COMPILER_DIR}/models)
set(COMPILER_RUNTIME_DIR ${COMPILER_DIR}/compiletime)

BISON_TARGET(Parser
        ${PARSER_DIR}/parser.y
        ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        COMPILE_FLAGS "-d"
)

FLEX_TARGET(Lexer
        ${PARSER_DIR}/lexer.l
        ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR})

# AST viewer
add_executable(umka_parser_lexer
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        ${PARSER_DIR}/main_ast.cpp
)

target_include_directories(umka_parser_lexer PRIVATE
        ${GEN_DIR}
        ${PARSER_DIR}
)

# Compiler
add_executable(umka_compiler
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        ${COMPILER_RUNTIME_DIR}/bytecode_generator.cpp
        ${COMPILER_DIR}/main_compiler.cpp
)

target_include_directories(umka_compiler PRIVATE
        ${GEN_DIR}
        ${PARSER_DIR}
        ${COMPILER_DIR}
        ${COMPILER_MODELS_DIR}
        ${COMPILER_RUNTIME_DIR}
)
#JIT

add_library(umka_jit
        UMKA-JIT/jit_runner.h
        UMKA-JIT/optimizations/base_optimization.h
        UMKA-JIT/optimizations/const_folding.h
        UMKA-JIT/optimizations/dce.h
        UMKA-JIT/jit_manager.cpp
        UMKA-JIT/jit_manager.h
)

target_include_directories(umka_jit PUBLIC
        ${CMAKE_SOURCE_DIR}/UMKA-JIT
        ${CMAKE_SOURCE_DIR}/UMKA-JIT/optimizations

        ${CMAKE_SOURCE_DIR}/UMKA-VM
        ${CMAKE_SOURCE_DIR}/UMKA-VM/model
        ${CMAKE_SOURCE_DIR}/UMKA-VM/parser
        ${CMAKE_SOURCE_DIR}/UMKA-VM/runtime
)

# VM
add_executable(umka_vm
    UMKA-VM/main.cpp
    UMKA-VM/garbage_collector/garbage_collector.h
    UMKA-VM/model/model.h
    UMKA-VM/model/model.cpp
    UMKA-VM/parser/command_parser.h
    UMKA-VM/parser/command_parser.cpp
    UMKA-VM/runtime/operations.h
    UMKA-VM/runtime/stack_machine.h
    UMKA-VM/runtime/profiler.h
    UMKA-VM/runtime/standart_funcs.cpp
)

target_include_directories(umka_vm PRIVATE UMKA-VM UMKA-JIT)

target_link_libraries(umka_vm PRIVATE umka_jit)


# VM tests
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(stack_machine_tests
        UMKA-VM/tests/stack_machine_test.cpp
        UMKA-VM/runtime/standart_funcs.cpp
        UMKA-VM/model/model.cpp
)

# ==========================
# JIT TESTS
# ==========================
add_executable(jit_tests
        UMKA-JIT/tests/jit_tests.cpp

        UMKA-JIT/jit_runner.cc
        UMKA-VM/model/model.cpp
        UMKA-JIT/jit_manager.cpp
        UMKA-JIT/jit_manager.h
)

target_include_directories(jit_tests PRIVATE
        UMKA-JIT
        UMKA-JIT/optimizations
        UMKA-VM
        UMKA-VM/model
        UMKA-VM/parser
        UMKA-VM/runtime
)

target_link_libraries(jit_tests PRIVATE gtest_main)

set_target_properties(jit_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test
)

add_test(NAME jit_tests COMMAND jit_tests)
 #=====
set_target_properties(stack_machine_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test
)

target_include_directories(stack_machine_tests PRIVATE UMKA-VM)
target_link_libraries(stack_machine_tests PRIVATE gtest_main)

enable_testing()
add_test(NAME stack_machine_tests COMMAND stack_machine_tests)