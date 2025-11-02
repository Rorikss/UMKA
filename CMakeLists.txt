cmake_minimum_required(VERSION 3.16)
project(SimpleLang LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- подключаем Bison и Flex ---
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

# --- пути к исходникам ---
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# --- генерируем parser (Bison) ---
BISON_TARGET(Parser
        ${SRC_DIR}/parser.y
        ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        COMPILE_FLAGS "-d"
)

# --- генерируем lexer (Flex) ---
FLEX_TARGET(Lexer
        ${SRC_DIR}/lexer.l
        ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
)

# --- устанавливаем зависимость: lexer зависит от parser ---
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# --- создаём исполняемый файл ---
add_executable(simplelang
        ${BISON_Parser_OUTPUTS}
        ${FLEX_Lexer_OUTPUTS}
        ${SRC_DIR}/main.cpp
)

# --- добавляем include-пути ---
# (чтобы lexer мог найти parser.tab.h)
target_include_directories(simplelang PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${SRC_DIR}
)

# --- диагностика ---
message(STATUS "Bison executable: ${BISON_EXECUTABLE}")
message(STATUS "Flex executable:  ${FLEX_EXECUTABLE}")
message(STATUS "Bison outputs:    ${BISON_Parser_OUTPUTS}")
message(STATUS "Flex outputs:     ${FLEX_Lexer_OUTPUTS}")
