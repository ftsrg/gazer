SET(TEST_SOURCES
    TypeTest.cpp
    SymbolTableTest.cpp
    core_tests.cpp
)

add_executable(CoreTest ${TEST_SOURCES})
target_link_libraries(CoreTest ${GTEST_BOTH_LIBRARIES} GazerCore)
add_test(GazerCoreTest CoreTest)