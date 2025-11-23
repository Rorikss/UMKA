cmake_minimum_required(VERSION 3.16)
project(SimpleLang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(PARSER_DIR ${CMAKE_SOURCE_DIR}/lexerAndParser)
set(COMPILER_DIR ${CMAKE_SOURCE_DIR}/UMKA-C)
set(MODELS_DIR ${COMPILER_DIR}/models)
set(RUNTIME_DIR ${COMPILER_DIR}/runtime)

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

# Compiler - ОБНОВЛЕННЫЙ СПИСОК ФАЙЛОВ
add_executable(umka_compiler
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        ${RUNTIME_DIR}/bytecode_generator.cpp
        ${COMPILER_DIR}/main_compiler.cpp
)

target_include_directories(umka_compiler PRIVATE
        ${GEN_DIR}
        ${PARSER_DIR}
        ${COMPILER_DIR}
        ${MODELS_DIR}
        ${RUNTIME_DIR}
)