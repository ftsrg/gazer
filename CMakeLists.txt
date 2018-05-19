cmake_minimum_required(VERSION 3.8)
project(gazer)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Project settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Wall -Wno-missing-braces -Wmissing-field-initializers ")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Compile dependencies
#add_subdirectory(vendor/mstch)
#include_directories(${mstch_INCLUDE_DIR})

add_subdirectory(vendor/fmt)
include_directories(vendor/fmt/include)

# Get LLVM
find_package(LLVM 6.0 REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(LLVM_LIBS support)

# Project directories

include_directories(include)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(unittest)
