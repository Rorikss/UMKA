cmake_minimum_required(VERSION 3.16)
project(UMKA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(PARSER_DIR ${CMAKE_SOURCE_DIR}/UMKA-LP)
set(COMPILER_DIR ${CMAKE_SOURCE_DIR}/UMKA-C)
set(COMPILER_MODELS_DIR ${COMPILER_DIR}/models)
set(COMPILER_RUNTIME_DIR ${COMPILER_DIR}/runtime)

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

# VM
add_executable(umka_vm
    UMKA-VM/main.cpp
    UMKA-VM/model/model.h
    UMKA-VM/parser/command_parser.h
    UMKA-VM/parser/command_parser.cpp
    UMKA-VM/runtime/operations.h
    UMKA-VM/runtime/stack_machine.h
    UMKA-VM/runtime/profiler.h
    UMKA-VM/runtime/standart_funcs.cpp
)

target_include_directories(umka_vm PRIVATE UMKA-VM)

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
)

set_target_properties(stack_machine_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/test
)

target_include_directories(stack_machine_tests PRIVATE UMKA-VM)
target_link_libraries(stack_machine_tests PRIVATE gtest_main)

enable_testing()
add_test(NAME stack_machine_tests COMMAND stack_machine_tests)