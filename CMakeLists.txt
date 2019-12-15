cmake_minimum_required(VERSION 3.8)
project(gazer)

include(ExternalProject)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Project settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -Wall -Wno-missing-braces -Wmissing-field-initializers -Werror=return-type")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# AddressSanitizer
option(GAZER_ENABLE_SANITIZER "Enable sanitizer flags for debug builds" OFF)

if(GAZER_ENABLE_SANITIZER)
    if (CMAKE_BUILD_TYPE MATCHES Debug AND ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")))
        message(STATUS "Adding sanitizer flags")
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined")
    else()
        message(STATUS "Sanitizer flags are only available with clang/gcc and only for debug builds")
    endif()
endif()

# Get LLVM
find_package(LLVM 9.0 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# Get boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

add_definitions(-DBOOST_NO_RTTI -DBOOST_EXCEPTION_DISABLE -DBOOST_NO_EXCEPTIONS)

# Project directories
include_directories(include)

add_subdirectory(src)
add_subdirectory(tools)

option(GAZER_ENABLE_UNIT_TESTS "Enable unit tests" ON)

if (GAZER_ENABLE_UNIT_TESTS)
    add_subdirectory(unittest)
endif()

set(GAZER_CLANG_TEST_COMPILER "clang" CACHE STRING "Clang compiler path for functional tests")

add_custom_target(check-functional
    COMMAND GAZER_TOOLS_DIR=${CMAKE_BINARY_DIR}/tools lit -v --timeout=60 ${CMAKE_CURRENT_LIST_DIR}/test
    DEPENDS gazer-bmc gazer-cfa gazer-theta
)

find_program(CLANG_TIDY NAMES "clang-tidy")
if(NOT CLANG_TIDY)
	message(STATUS "clang-tidy was not found")
else()
	message(STATUS "Found clang-tidy: ${CLANG_TIDY}")
    file(GLOB_RECURSE ALL_SOURCE_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)
    get_property(INCLUDE_DIRS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
    add_custom_target(
        clang-tidy
        COMMAND ${CLANG_TIDY}
        -p ${CMAKE_BINARY_DIR}/compile_commands.json
        ${ALL_SOURCE_FILES}
    )
endif()
