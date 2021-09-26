set(SOURCE_FILES
    BoundedModelChecker.cpp
    BmcTrace.cpp
)

add_library(GazerVerifier SHARED ${SOURCE_FILES})
target_link_libraries(GazerVerifier GazerCore GazerAutomaton GazerTrace)
