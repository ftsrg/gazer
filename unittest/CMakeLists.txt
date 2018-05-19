# Get gtest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Enable unit testing
enable_testing()

add_subdirectory(Core)