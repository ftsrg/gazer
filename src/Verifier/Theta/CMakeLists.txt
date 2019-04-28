set(SOURCE_FILES
    ThetaCfaGenerator.cpp
)

add_library(GazerVerifierTheta ${SOURCE_FILES})
target_link_libraries(GazerVerifierTheta GazerCore)
