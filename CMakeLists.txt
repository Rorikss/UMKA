cmake_minimum_required(VERSION 3.10)
project(UMKA LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(umka_vm
    UMKA-VM/main.cpp
)

target_include_directories(umka_vm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM/model
    ${CMAKE_CURRENT_SOURCE_DIR}/UMKA-VM/runtime
)