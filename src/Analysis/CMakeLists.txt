set(SOURCE_FILES
    MemoryObject.cpp
    MemoryModel.cpp
    LoopExtendedCallGraph.cpp)

add_library(GazerAnalysis ${SOURCE_FILES})
target_link_libraries(GazerAnalysis GazerCore GazerLLVM)
