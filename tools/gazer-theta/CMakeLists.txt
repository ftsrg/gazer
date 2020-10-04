set(LIB_SOURCE_FILES
        lib/ThetaCfaGenerator.cpp
    lib/ThetaExpr.cpp
    lib/ThetaVerifier.cpp
    lib/ThetaCfaWriterPass.cpp)

set(TOOL_SOURCE_FILES
    gazer-theta.cpp
)

add_library(GazerBackendTheta ${LIB_SOURCE_FILES})
target_link_libraries(GazerBackendTheta GazerLLVM)

add_executable(gazer-theta ${TOOL_SOURCE_FILES})

target_link_libraries(gazer-theta GazerBackendTheta)