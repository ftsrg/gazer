set(SOURCE_FILES
    Analysis/MemoryObject.cpp
    Analysis/DummyMemoryModel.cpp
    Analysis/VariableRoles.cpp
    Transform/InlineGlobalVariablesPass.cpp
    Transform/CombineErrorCalls.cpp
    Transform/LiftErrorsPass.cpp
    Instrumentation/MarkFunctionEntries.cpp
    Instrumentation/Check.cpp
    Instrumentation/DefaultChecks.cpp
    Instrumentation/Intrinsics.cpp
    Trace/TestHarnessGenerator.cpp
    Utils/LLVMType.cpp
    Automaton/ModuleToAutomata.cpp
    Automaton/InstToExpr.cpp
    Verifier/BmcPass.cpp
    LLVMTraceBuilder.cpp
    TypeTranslator.cpp
    LLVMFrontendSettings.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS support core irreader transformutils scalaropts ipo)
message(STATUS "Using LLVM libraries: ${LLVM_LIBS}")

add_library(GazerLLVM ${SOURCE_FILES} )
target_link_libraries(GazerLLVM ${LLVM_LIBS} GazerCore GazerTrace GazerZ3Solver GazerAutomaton GazerVerifier)
