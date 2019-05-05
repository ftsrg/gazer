SET(TEST_SOURCES
        Automaton/ModuleToAutomataTest.cpp
        )

add_executable(GazerLLVMTest ${TEST_SOURCES})
target_link_libraries(GazerLLVMTest ${GTEST_BOTH_LIBRARIES} GazerLLVM)
add_test(GazerLLVMTest GazerLLVMTest)