SET(TEST_SOURCES
    BoundedModelCheckerTest.cpp
)

add_executable(GazerVerifierTest ${TEST_SOURCES})
target_link_libraries(GazerVerifierTest gtest_main GazerVerifier GazerZ3Solver)
add_test(NAME GazerVerifierTest COMMAND GazerVerifierTest)
