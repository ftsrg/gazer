set(SOURCE_FILES
    Cfa.cpp
    CfaPrinter.cpp
    CallGraph.cpp
    CfaUtils.cpp
    CfaInline.cpp
    RecursiveToCyclicCfa.cpp
    CfaClone.cpp)

add_library(GazerAutomaton SHARED ${SOURCE_FILES})
target_link_libraries(GazerAutomaton GazerCore)