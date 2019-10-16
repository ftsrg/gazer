set(SOURCE_FILES
    Z3Solver.cpp
)

add_library(GazerZ3Solver ${SOURCE_FILES})
target_link_libraries(GazerZ3Solver GazerCore ${Z3_LIBRARIES})
