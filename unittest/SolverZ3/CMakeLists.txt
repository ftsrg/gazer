SET(TEST_SOURCES
    Z3SolverTest.cpp
)

add_library(SolverZ3Test ${TEST_SOURCES})
target_link_libraries(SolverZ3Test ${GTEST_BOTH_LIBRARIES} GazerCore GazerZ3Solver)
add_test(GazerSolverZ3Test SolverZ3Test)