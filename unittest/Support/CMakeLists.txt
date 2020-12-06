SET(TEST_SOURCES
    SExprTest.cpp
)

add_executable(GazerSupportTest ${TEST_SOURCES})
target_link_libraries(GazerSupportTest gtest_main GazerLLVM)
add_test(GazerSupportTest GazerSupportTest)
