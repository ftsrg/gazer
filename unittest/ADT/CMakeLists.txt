SET(TEST_SOURCES
    IntersectionDifferenceTest.cpp
    GraphTest.cpp
    IteratorTest.cpp
)

add_executable(GazerAdtTest ${TEST_SOURCES})
target_link_libraries(GazerAdtTest gtest_main GazerSupport)
add_test(NAME GazerAdtTest COMMAND GazerAdtTest)