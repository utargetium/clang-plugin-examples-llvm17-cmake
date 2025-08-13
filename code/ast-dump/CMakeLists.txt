cmake_minimum_required(VERSION 3.14)

project(ast-dump)
add_executable(ast-dump ast-dump.cpp)
add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -fno-exceptions -fno-rtti)

# ---------------------------------------------------

set(CMAKE_CXX_COMPILER clang++-17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_VERBOSE_MAKEFILE ON) # for debugging

# ---------------------------------------------------

execute_process(
    COMMAND llvm-config-17 --libdir
    OUTPUT_VARIABLE LLVM_LIBRARY_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND llvm-config-17 --includedir
    OUTPUT_VARIABLE LLVM_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# ---------------------------------------------------

target_include_directories(ast-dump PRIVATE ${LLVM_INCLUDE_DIR})
target_link_directories(ast-dump PRIVATE ${LLVM_LIBRARY_DIR})
target_link_libraries(ast-dump clang)
