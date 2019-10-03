SET(TEST_SOURCES
    ThetaExprPrinterTest.cpp
)

add_executable(GazerToolsBackendThetaTest ${TEST_SOURCES})
target_link_libraries(GazerToolsBackendThetaTest gtest_main GazerLLVM GazerBackendTheta)
add_test(GazerToolsBackendThetaTest GazerToolsBackendThetaTest)