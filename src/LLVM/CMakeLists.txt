set(MODULE_SOURCE_FILES
    Transform/InlineGlobalVariablesPass.cpp
    Transform/UndefToNondet.cpp
    Transform/LiftErrorsPass.cpp
    Transform/NormalizeVerifierCalls.cpp
    Transform/BackwardSlicer.cpp
    Transform/Inline.cpp
    Transform/TransformUtils.cpp
    Instrumentation/MarkFunctionEntries.cpp
    Instrumentation/Check.cpp
    Instrumentation/Intrinsics.cpp
    Trace/TestHarnessGenerator.cpp
    Automaton/ModuleToAutomata.cpp
    Automaton/InstToExpr.cpp
    Automaton/TranslationSupport.cpp
    Automaton/SpecialFunctions.cpp
    Memory/MemoryObject.cpp
    Memory/FlatMemoryModel.cpp
    Memory/HavocMemoryModel.cpp
    Memory/MemoryModel.cpp
    Memory/MemorySSA.cpp
    Memory/MemoryUtils.cpp
    Memory/MemoryInstructionHandler.cpp
    Automaton/AutomatonPasses.cpp
    Automaton/ExtensionPoints.cpp
    Automaton/ValueOrMemoryObject.cpp
    Analysis/PDG.cpp
    TypeTranslator.cpp
    Instrumentation/Checks/AssertionFailCheck.cpp
    Instrumentation/Checks/DivisionByZeroCheck.cpp
    Instrumentation/Checks/SignedIntegerOverflowCheck.cpp Transform/LoopExitCanonizationPass.cpp
)

set(SOURCE_FILES
    LLVMTraceBuilder.cpp
    LLVMFrontendSettings.cpp
    LLVMFrontend.cpp
    ClangFrontend.cpp
    FrontendConfig.cpp
    LLVMFrontendSettingsProviderPass.cpp)

# module for LLVM opt integration (use like opt -load src/LLVM/libGazerLLVMBare.so -gazer-... test.ll)
add_library(GazerLLVMBare SHARED ${MODULE_SOURCE_FILES})
target_link_libraries(GazerLLVMBare GazerCore GazerAutomaton GazerTrace GazerSupport)

llvm_map_components_to_libnames(GAZER_LLVM_LIBS core irreader transformutils scalaropts ipo)
message(STATUS "Using LLVM libraries: ${GAZER_LLVM_LIBS}")

# original target, with libraries linked
# add_library must have options
add_library(GazerLLVM SHARED ${SOURCE_FILES})
target_link_libraries(GazerLLVM GazerLLVMBare ${GAZER_LLVM_LIBS})
