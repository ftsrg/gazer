set(SOURCE_FILES
    gazer-bmc.cpp
)

set(LIB_SOURCE_FILES
    lib/BoundedModelChecker.cpp
    lib/BmcTrace.cpp
)

add_library(GazerBackendBmc ${LIB_SOURCE_FILES})
target_link_libraries(GazerBackendBmc GazerLLVM)

add_executable(gazer-bmc ${SOURCE_FILES})
target_link_libraries(gazer-bmc GazerLLVM GazerZ3Solver GazerBackendBmc)
