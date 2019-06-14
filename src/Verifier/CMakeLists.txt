set(SOURCE_FILES
    BoundedModelChecker.cpp
)

add_library(GazerVerifier ${SOURCE_FILES})
target_link_libraries(GazerVerifier GazerCore GazerAutomaton GazerTrace)
