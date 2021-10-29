enable_testing()

include(FetchContent)

# Load GoogleTest
FetchContent_Declare(
    gtest
    GIT_REPOSITORY  "https://github.com/google/googletest"
    GIT_TAG         master
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gtest)

# Add tests
add_subdirectory(ADT)
add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(LLVM)
add_subdirectory(Support)
add_subdirectory(Verifier)
add_subdirectory(tools/gazer-theta)

# Only add tests for requested targets
if ("z3" IN_LIST GAZER_ENABLE_SOLVERS)
    add_subdirectory(SolverZ3)
endif()

add_custom_target(check-unit
    COMMAND ctest --output-on-failure
)

add_dependencies(check-unit
    GazerAdtTest
    GazerCoreTest
    GazerLLVMTest
    GazerAutomatonTest
    GazerSolverZ3Test
    GazerToolsBackendThetaTest
    GazerVerifierTest
    GazerSupportTest
)
