set(SOURCE_FILES
    Analysis/ProgramDependence.cpp
    Analysis/TopologicalSort.cpp
    Analysis/MemoryObject.cpp
    Analysis/DummyMemoryModel.cpp
    Transform/BackwardSlicer.cpp
    Transform/InlineGlobalVariablesPass.cpp
    Transform/CombineErrorCalls.cpp
    Instrumentation/MarkFunctionEntries.cpp
    Instrumentation/InsertLastAddress.cpp
    Instrumentation/Check.cpp
    Instrumentation/DefaultChecks.cpp
    Instrumentation/Intrinsics.cpp
    TestGenerator/TestGenerator.cpp
    Utils/LLVMType.cpp
    Automaton/ModuleToAutomata.cpp
    Verifier/BmcPass.cpp
    LLVMTraceBuilder.cpp
    TypeTranslator.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS support core irreader transformutils scalaropts ipo)
message(STATUS "Using LLVM libraries: ${LLVM_LIBS}")

add_library(GazerLLVM ${SOURCE_FILES} )
target_link_libraries(GazerLLVM ${LLVM_LIBS} GazerCore GazerTrace GazerZ3Solver GazerAutomaton GazerVerifier)
