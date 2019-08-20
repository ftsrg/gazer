SET(TEST_SOURCES
    CfaTest.cpp
    CfaPrinterTest.cpp
)

add_executable(GazerAutomatonTest ${TEST_SOURCES})
target_link_libraries(GazerAutomatonTest gtest_main GazerCore GazerAutomaton)
add_test(GazerAutomatonTest GazerAutomatonTest)