set(SOURCE_FILES
    Z3Solver.cpp
)

find_package(Z3 REQUIRED)

add_library(GazerZ3Solver ${SOURCE_FILES})
target_link_libraries(GazerZ3Solver GazerCore)
