cmake_minimum_required(VERSION 3.16)
project(SimpleLang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- подключаем Bison и Flex ---
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# --- пути к исходникам: переехали в lexerAndParser ---
set(PARSER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lexerAndParser)

# --- генерируем parser (Bison) ---
BISON_TARGET(Parser
        ${PARSER_DIR}/parser.y
        ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        COMPILE_FLAGS "-d"
)

# --- генерируем lexer (Flex) ---
FLEX_TARGET(Lexer
        ${PARSER_DIR}/lexer.l
        ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

# --- зависимость: lexer зависит от parser ---
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# --- создаём исполняемый файл с нужным именем (CMake сам добавит .exe на Windows) ---
add_executable(umka_parser_lexer
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        ${PARSER_DIR}/main.cpp
)

# --- include-пути (чтобы lexer мог найти сгенерированный parser header) ---
target_include_directories(umka_parser_lexer PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${PARSER_DIR}
)

# --- диагностика ---
message(STATUS "Bison executable: ${BISON_EXECUTABLE}")
message(STATUS "Flex executable:  ${FLEX_EXECUTABLE}")
message(STATUS "Bison outputs:    ${BISON_Parser_OUTPUTS}")
message(STATUS "Flex outputs:     ${FLEX_Lexer_OUTPUTS}")
