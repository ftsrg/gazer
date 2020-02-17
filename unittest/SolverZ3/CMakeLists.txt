SET(TEST_SOURCES
    Z3SolverTest.cpp
    Z3ModelTest.cpp
)

add_executable(GazerSolverZ3Test ${TEST_SOURCES})
target_link_libraries(GazerSolverZ3Test gtest_main GazerCore GazerZ3Solver)
add_test(GazerSolverZ3Test GazerSolverZ3Test)