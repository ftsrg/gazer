set(SOURCE_FILES
    gazer-bmc.cpp
)

add_executable(gazer-bmc ${SOURCE_FILES})
target_link_libraries(gazer-bmc GazerCore GazerZ3Solver GazerBMC GazerLLVM)