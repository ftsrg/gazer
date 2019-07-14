SET(TEST_SOURCES
    Analysis/MemoryObjectTest.cpp
    Automaton/ModuleToAutomataTest.cpp
)

add_executable(GazerLLVMTest ${TEST_SOURCES})
target_link_libraries(GazerLLVMTest gtest_main GazerLLVM)
add_test(GazerLLVMTest GazerLLVMTest)