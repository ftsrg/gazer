SET(TEST_SOURCES
    Memory/MemoryObjectTest.cpp
    Analysis/VariableRoleTest.cpp
    Automaton/InstToExprTest.cpp
    Trace/TestHarnessGeneratorTest.cpp
)

add_executable(GazerLLVMTest ${TEST_SOURCES})
target_link_libraries(GazerLLVMTest gtest_main GazerLLVM)
add_test(GazerLLVMTest GazerLLVMTest)