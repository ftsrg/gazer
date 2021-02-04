add_subdirectory(Verifier)

set(SOURCE_FILES
    gazer-bmc.cpp
)

add_executable(gazer-bmc ${SOURCE_FILES})
target_link_libraries(gazer-bmc GazerLLVM GazerZ3Solver GazerVerifier)