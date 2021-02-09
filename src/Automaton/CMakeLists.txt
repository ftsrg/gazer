set(SOURCE_FILES
    Cfa.cpp
    CfaPrinter.cpp
    CallGraph.cpp
    CfaUtils.cpp
    RecursiveToCyclicCfa.cpp
)

add_library(GazerAutomaton SHARED ${SOURCE_FILES})
target_link_libraries(GazerAutomaton GazerCore)

set_target_properties(GazerAutomaton PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib"
)