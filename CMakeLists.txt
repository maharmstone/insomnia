cmake_minimum_required(VERSION 3.16)
project(SleepCheckPlugin)

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
