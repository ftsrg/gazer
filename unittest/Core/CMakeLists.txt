SET(TEST_SOURCES
    TypeTest.cpp
    SymbolTableTest.cpp
    ExprTest.cpp
    Expr/MatcherTest.cpp
    core_tests.cpp
)

add_executable(GazerCoreTest ${TEST_SOURCES})
target_link_libraries(GazerCoreTest ${GTEST_BOTH_LIBRARIES} GazerCore)
add_test(GazerCoreTest GazerCoreTest)