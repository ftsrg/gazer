cmake_minimum_required(VERSION 3.8)
project(gazer)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Project settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -Wall -Wno-missing-braces -Wmissing-field-initializers -DLLVM_ENABLE_ABI_BREAKING_CHECKS=1")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Compile dependencies
#add_subdirectory(vendor/mstch)
#include_directories(${mstch_INCLUDE_DIR})

#add_subdirectory(vendor/fmt)
#include_directories(vendor/fmt/include)

# Get LLVM
find_package(LLVM 7.0 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(LLVM_LIBS support)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Add DSA and AssistDS
#include_directories(vendor/llvm-dsa/include)
#add_subdirectory(vendor/llvm-dsa)

# Add boost
find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIR})

# Project directories

include_directories(include)

add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(unittest)
