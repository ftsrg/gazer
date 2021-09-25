# Fetch Z3
set(Z3_DOWNLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/vendor/z3_download")

FetchContent_Declare(
    z3_download
    URL https://github.com/Z3Prover/z3/releases/download/z3-4.8.6/z3-4.8.6-x64-ubuntu-16.04.zip
    URL_HASH SHA1=1663a7825c45f24642045dfcc5e100d65c1b6b1e
    SOURCE_DIR ${Z3_DOWNLOAD_DIR}
)

message(STATUS "Fetching Z3")
FetchContent_MakeAvailable(z3_download)

set(Z3_INCLUDE_DIR "${Z3_DOWNLOAD_DIR}/include")
set(Z3_LIBRARY_DIR "${Z3_DOWNLOAD_DIR}/bin")

message(STATUS "Z3 includes are ${Z3_INCLUDE_DIR}")

add_library(z3 SHARED IMPORTED)
set_property(TARGET z3 PROPERTY IMPORTED_LOCATION "${Z3_LIBRARY_DIR}/libz3.so")
include_directories("${Z3_INCLUDE_DIR}")
add_dependencies(z3 z3_download)

set(SOURCE_FILES
    Z3Solver.cpp
    Z3Model.cpp
)

add_library(GazerZ3Solver SHARED ${SOURCE_FILES})
target_link_libraries(GazerZ3Solver GazerCore z3)
