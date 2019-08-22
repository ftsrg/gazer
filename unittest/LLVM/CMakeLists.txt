SET(TEST_SOURCES
    Analysis/MemoryObjectTest.cpp
    Automaton/ModuleToAutomataTest.cpp
    InstToExprTest.cpp
    GazerLLVMTest_main.cpp
)

add_executable(GazerLLVMTest ${TEST_SOURCES})
target_link_libraries(GazerLLVMTest gtest GazerLLVM "stdc++fs")
add_test(GazerLLVMTest GazerLLVMTest "${CMAKE_CURRENT_LIST_DIR}/resources")