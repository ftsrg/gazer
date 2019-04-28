SET(TEST_SOURCES
        CfaTest.cpp
)

add_executable(GazerAutomatonTest ${TEST_SOURCES})
target_link_libraries(GazerAutomatonTest ${GTEST_BOTH_LIBRARIES} GazerCore GazerAutomaton)
add_test(GazerAutomatonTest GazerAutomatonTest)