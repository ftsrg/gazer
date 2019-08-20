set(SOURCE_FILES
    ThetaCfaGenerator.cpp
)

add_library(GazerBackendTheta ${SOURCE_FILES})
target_link_libraries(GazerBackendTheta GazerCore GazerTrace)