cmake_minimum_required(VERSION 3.14)

project(using_vs_typedef)
add_executable(using_vs_typedef "using.cpp")
add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter -fno-exceptions -fno-rtti)

# ---------------------------------------------------

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use mold linker (replace 'mold' with 'lld' or 'gold' if needed)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=mold")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fuse-ld=mold")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fuse-ld=mold")

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

execute_process(
    COMMAND sh -c "llvm-config-17 --libs --system-libs | sed -E 's|-l(.*)$|\\1|'" # \1 -> \\1
    OUTPUT_VARIABLE LLVM_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND sh -c "ls -1 $(llvm-config-17 --libdir)/libclang*.a | sed -E 's|.*/lib(.*)\.a$|\\1|'" # \1 -> \\1
    OUTPUT_VARIABLE CLANG_LIBS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# -----------------------------------------------------------------

string(REPLACE "\n" ";" CLANG_LIBS "${CLANG_LIBS}")
string(REPLACE "\n" ";" LLVM_LIBS "${LLVM_LIBS}")

message(STATUS "LLVM_INCLUDE_DIR = ${LLVM_INCLUDE_DIR}")
message(STATUS "LLVM_LIBRARY_DIR = ${LLVM_LIBRARY_DIR}")
message(STATUS "LLVM_LIBS = ${LLVM_LIBS}")
message(STATUS "CLANG_LIBS = ${CLANG_LIBS}")

# -----------------------------------------------------------------

target_include_directories(using_vs_typedef PRIVATE ${LLVM_INCLUDE_DIR})
target_link_directories(using_vs_typedef PRIVATE ${LLVM_LIBRARY_DIR})
target_link_libraries(using_vs_typedef ${LLVM_LIBS} ${CLANG_LIBS})
