set(LIB_SOURCE_FILES
    lib/ThetaType.cpp
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

# TODO: Boost filesystem should be linked "properly" or dropped altogether
target_link_libraries(gazer-theta GazerBackendTheta -lboost_system -lboost_filesystem)