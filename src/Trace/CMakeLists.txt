set(SOURCE_FILES
    TextTraceWriter.cpp
    VerificationResult.cpp
    CorrectnessWitnessWriter.cpp
    ViolationWitnessWriter.cpp
)

add_library(GazerTrace SHARED ${SOURCE_FILES})
target_link_libraries(GazerTrace GazerCore)

set_target_properties(GazerTrace PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib"
)