cmake_minimum_required(VERSION 3.13)
project(gazer)

# Set version information

if(NOT DEFINED GAZER_VERSION_MAJOR)
  set(GAZER_VERSION_MAJOR 1)
endif()
if(NOT DEFINED GAZER_VERSION_MINOR)
  set(GAZER_VERSION_MINOR 4)
endif()
if(NOT DEFINED GAZER_VERSION_PATCH)
  set(GAZER_VERSION_PATCH 3)
endif()
if(NOT DEFINED GAZER_VERSION_SUFFIX)
  set(GAZER_VERSION_SUFFIX "")
endif()

if (NOT PACKAGE_VERSION)
  set(PACKAGE_VERSION
    "${GAZER_VERSION_MAJOR}.${GAZER_VERSION_MINOR}.${GAZER_VERSION_PATCH}${GAZER_VERSION_SUFFIX}")
endif()

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

## Coverage
################################################################################
option(GAZER_ENABLE_COVERAGE "Enable code coverage" OFF)
if (GAZER_ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "Adding coverage flags for Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate -fcoverage-mapping")
    elseif (CMAKE_COMPILER_IS_GNUCXX)
        message(STATUS "Adding coverage flags for GCC")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    else()
        message(STATUS "Coverage flags are only available with Clang or GCC")
    endif()
endif()

# Get LLVM
find_package(LLVM 12.0 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

# If `GAZER_SHARED_LLVM` is on, we want to link against LLVM dynamically.
# If it is set to off, we want to map components to static library names
# and link against those.
option(GAZER_SHARED_LLVM "Link against shared libLLVM.so" OFF)

function(require_llvm_libraries outVar)
  if (GAZER_SHARED_LLVM)
    message(STATUS "Searching for shared libLLVM in ${LLVM_LIBRARY_DIR}")
    find_library(
      LLVM_LIBS
      NAMES LLVM-9
      PATHS ${LLVM_LIBRARY_DIRS}
      NO_DEFAULT_PATH
    )
    message(STATUS "Found shared libLLVM in ${LLVM_LIBS}")
    set(${outVar} ${LLVM_LIBS} PARENT_SCOPE)
  else()
    llvm_map_components_to_libnames(LLVM_LIBRARY_NAMES ${ARGN})
    set(${outVar} ${LLVM_LIBRARY_NAMES} PARENT_SCOPE)
  endif()
endfunction()

# Get boost
include(FetchContent)

FetchContent_Declare(
    Boost
    URL https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.bz2
    URL_HASH SHA256=f0397ba6e982c4450f27bf32a2a83292aba035b827a5623a14636ea583318c41
)
FetchContent_GetProperties(Boost)

if(NOT Boost_POPULATED)
    message(STATUS "Fetching Boost")
    FetchContent_Populate(Boost)
    message(STATUS "Fetching Boost - done")
endif()

message("Boost include directories are ${boost_SOURCE_DIR}")
add_definitions(-DBOOST_NO_RTTI -DBOOST_EXCEPTION_DISABLE -DBOOST_NO_EXCEPTIONS -DBOOST_NO_IOSTREAM)
include_directories(${boost_SOURCE_DIR})


# Project directories
set(GAZER_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include")
set(GAZER_MAIN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")

include_directories(${GAZER_INCLUDE_DIR} ${GAZER_MAIN_INCLUDE_DIR})

# Find out which solvers are enabled
set(GAZER_ENABLE_SOLVERS "z3" CACHE STRING "Semicolon-separated list of solvers to build")

add_subdirectory(src)
add_subdirectory(tools)

# Generate the configuration file
configure_file(
  ${GAZER_INCLUDE_DIR}/gazer/Config/gazer-config.h.cmake
  ${GAZER_MAIN_INCLUDE_DIR}/gazer/Config/gazer-config.h)

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
