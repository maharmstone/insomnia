cmake_minimum_required(VERSION 3.16)

# This is a Clang plugin project — ensure we use clang as the C compiler
# so that -Xclang flags work for the demo executable.
find_program(CLANG_C_COMPILER clang REQUIRED)
set(CMAKE_C_COMPILER "${CLANG_C_COMPILER}" CACHE FILEPATH "C compiler" FORCE)

project(SleepCheckPlugin C CXX)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_library(SleepCheckPlugin MODULE SleepCheckPlugin.cpp)
target_compile_features(SleepCheckPlugin PRIVATE cxx_std_17)

# Clang plugins must not link against libclang/libLLVM on Linux
if(NOT APPLE)
  target_link_options(SleepCheckPlugin PRIVATE "LINKER:--unresolved-symbols=ignore-all")
endif()

set_target_properties(SleepCheckPlugin PROPERTIES
  PREFIX ""
  SUFFIX ".so"
)

# Demo executable that exercises nosleep_enter/nosleep_exit via the plugin.
# The plugin is loaded via -Xclang flags so clang runs the sleep-check
# analysis during compilation.
add_executable(spinlock_demo test/spinlock_demo.c)
add_dependencies(spinlock_demo SleepCheckPlugin)
target_compile_options(spinlock_demo PRIVATE
  "SHELL:-Xclang -load -Xclang $<TARGET_FILE:SleepCheckPlugin>"
  "SHELL:-Xclang -add-plugin -Xclang sleep-check"
)
