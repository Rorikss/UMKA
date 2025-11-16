cmake_minimum_required(VERSION 3.16)
project(SimpleLang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- Bison и Flex ---
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(PARSER_DIR lexerAndParser)
set(COMPILER_DIR UMKA-C)

# --- Генерация parser (Bison) ---
BISON_TARGET(Parser
        ${PARSER_DIR}/parser.y
        ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        COMPILE_FLAGS "-d"
)

# --- Генерация lexer (Flex) ---
FLEX_TARGET(Lexer
        ${PARSER_DIR}/lexer.l
        ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# ==========================================================
# 1) Цель: AST viewer (только парсер + лексер + main_ast)
# ==========================================================
add_executable(umka_parser_lexer
        ${BISON_Parser_OUTPUTS}     # parser.cpp + parser.hpp
        ${FLEX_Lexer_OUTPUTS}       # lexer.cpp
        ${PARSER_DIR}/main_ast.cpp
)

target_include_directories(umka_parser_lexer PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # parser.hpp
        ${PARSER_DIR}
)

# ==========================================================
# 2) Цель: Компилятор → генерирует .bin
# ==========================================================
add_executable(umka_compiler
        ${BISON_Parser_OUTPUTS}      # parser.cpp
        ${FLEX_Lexer_OUTPUTS}        # lexer.cpp
        ${COMPILER_DIR}/BytecodeGenerator.cpp
        ${COMPILER_DIR}/main_compiler.cpp
)

target_include_directories(umka_compiler PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}  # parser.hpp
        ${PARSER_DIR}
        ${COMPILER_DIR}
)
