set(SOURCE_FILES
    CorrectnessWitnessWriter.cpp
    ViolationWitnessWriter.cpp
)

add_library(GazerWitness SHARED ${SOURCE_FILES})
target_link_libraries(GazerTrace)