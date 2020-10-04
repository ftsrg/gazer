add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(LLVM)
add_subdirectory(Trace)
add_subdirectory(Verifier)
add_subdirectory(Support)

# Add requested solvers
if ("z3" IN_LIST GAZER_ENABLE_SOLVERS)
    add_subdirectory(SolverZ3)
endif()
