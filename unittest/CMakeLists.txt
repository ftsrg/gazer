# Get gtest
#find_package(GTest REQUIRED)

include(ExternalProject)

set(GOOGLETEST_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-src")
set(GOOGLETEST_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

ExternalProject_Add(googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           master
  SOURCE_DIR        "${GOOGLETEST_SOURCE_DIR}"
  BINARY_DIR        "${GOOGLETEST_BINARY_DIR}"
  CONFIGURE_COMMAND ""
  BUILD_COMMAND     ""
  INSTALL_COMMAND   ""
  TEST_COMMAND      ""
)

add_subdirectory(${GOOGLETEST_SOURCE_DIR} "${GOOGLETEST_BINARY_DIR}")
include_directories(${GOOGLETEST_SOURCE_DIR}/googletest/include)

add_subdirectory(Core)
add_subdirectory(Automaton)
add_subdirectory(SolverZ3)

add_custom_target(check-unit
  COMMAND ctest --output-on-failure
)
add_dependencies(check-unit GazerCoreTest GazerAutomatonTest GazerSolverZ3Test)