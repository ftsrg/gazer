enable_testing()
ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           master
  SOURCE_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-src"
  BINARY_DIR        "${CMAKE_CURRENT_BINARY_DIR}/googletest-build"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-src")
set(GOOGLETEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

add_subdirectory(${GOOGLETEST_SOURCE_DIR}
                 ${GOOGLETEST_BINARY_DIR}
                 EXCLUDE_FROM_ALL)

add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(SolverZ3)
add_subdirectory(LLVM)
#add_subdirectory(Support)
add_subdirectory(tools/gazer-theta)

add_custom_target(check-unit
  COMMAND ctest --output-on-failure
)

add_dependencies(check-unit GazerCoreTest GazerLLVMTest GazerAutomatonTest GazerSolverZ3Test GazerToolsBackendThetaTest)
