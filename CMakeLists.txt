cmake_minimum_required(VERSION 3.10)
project(UMKA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(umka_vm
    UMKA-VM/main.cpp
    UMKA-VM/model/model.h
    UMKA-VM/runtime/command_parser.h
    UMKA-VM/runtime/operations.h
    UMKA-VM/runtime/stack_machine.h
    UMKA-VM/runtime/standart_funcs.h
)

target_include_directories(umka_vm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM/model
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM/runtime/standart_funcs.h
)