SET(TEST_SOURCES
    Z3SolverTest.cpp
)

add_executable(GazerSolverZ3Test ${TEST_SOURCES})
target_link_libraries(GazerSolverZ3Test ${GTEST_BOTH_LIBRARIES} GazerCore GazerZ3Solver)
add_test(GazerSolverZ3Test GazerSolverZ3Test)