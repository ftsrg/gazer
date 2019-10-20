set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-src")
set(GOOGLETEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

include_directories(${GOOGLETEST_SOURCE_DIR}/googletest/include)

add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(SolverZ3)
add_subdirectory(LLVM)
add_subdirectory(Support)
add_subdirectory(tools/gazer-theta)

add_custom_target(check-unit
  COMMAND ctest --output-on-failure
)

add_dependencies(check-unit GazerCoreTest GazerLLVMTest GazerAutomatonTest GazerSolverZ3Test GazerToolsBackendThetaTest)
