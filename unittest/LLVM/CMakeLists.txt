SET(TEST_SOURCES
    Analysis/MemoryObjectTest.cpp
    Analysis/VariableRoleTest.cpp
    Automaton/ModuleToAutomataTest.cpp
    Automaton/InstToExprTest.cpp
    Trace/TestHarnessGeneratorTest.cpp
    GazerLLVMTest_main.cpp
)

add_executable(GazerLLVMTest ${TEST_SOURCES})
target_link_libraries(GazerLLVMTest gtest GazerLLVM "stdc++fs")
add_test(GazerLLVMTest GazerLLVMTest "${CMAKE_CURRENT_LIST_DIR}/resources")