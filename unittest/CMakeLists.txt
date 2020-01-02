enable_testing()

set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-src")
set(GOOGLETEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

# Download and unpack googletest at configure time
configure_file(${PROJECT_SOURCE_DIR}/cmake/GoogleTestDownload.cmake googletest-download/CMakeLists.txt)
execute_process(
    COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
)
if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(
    COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download
)
if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${GOOGLETEST_SOURCE_DIR}
    ${GOOGLETEST_BINARY_DIR}
    EXCLUDE_FROM_ALL
)

# Add tests
add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(LLVM)
add_subdirectory(Support)
add_subdirectory(tools/gazer-theta)

# Only add tests for requested targets
if ("z3" IN_LIST GAZER_ENABLE_SOLVERS)
    add_subdirectory(SolverZ3)
endif()

if ("mathsat5" IN_LIST GAZER_ENABLE_SOLVERS)
    add_subdirectory(SolverMathSAT5)
endif()

add_custom_target(check-unit
    COMMAND ctest --output-on-failure
)

add_dependencies(check-unit
    GazerCoreTest
    GazerLLVMTest
    GazerAutomatonTest
    GazerSolverZ3Test
    GazerToolsBackendThetaTest
    GazerSupportTest
)
