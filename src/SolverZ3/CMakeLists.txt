set(SOURCE_FILES
    Z3Solver.cpp
    Z3Model.cpp
)

# Z3

set(Z3_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vendor/src/z3_download/include")
set(Z3_LIBRARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/vendor/src/z3_download/bin")

ExternalProject_Add(z3_download
    PREFIX vendor
    # Download
    URL https://github.com/Z3Prover/z3/releases/download/z3-4.8.6/z3-4.8.6-x64-ubuntu-16.04.zip
    URL_HASH SHA1=1663a7825c45f24642045dfcc5e100d65c1b6b1e
    BUILD_BYPRODUCTS "${Z3_LIBRARY_DIR}/libz3.so"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
)

message("Z3 includes are ${Z3_INCLUDE_DIR}")

# Z3
add_library(z3 SHARED IMPORTED)
set_property(TARGET z3 PROPERTY IMPORTED_LOCATION "${Z3_LIBRARY_DIR}/libz3.so")
include_directories("${Z3_INCLUDE_DIR}")
add_dependencies(z3 z3_download)

add_library(GazerZ3Solver SHARED ${SOURCE_FILES})
target_link_libraries(GazerZ3Solver GazerCore z3)
