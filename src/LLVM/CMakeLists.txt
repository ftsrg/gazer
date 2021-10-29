set(SOURCE_FILES
    Transform/InlineGlobalVariablesPass.cpp
    Transform/UndefToNondet.cpp
    Transform/LiftErrorsPass.cpp
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
    LLVMTraceBuilder.cpp
    TypeTranslator.cpp
    LLVMFrontendSettings.cpp
    LLVMFrontend.cpp
    ClangFrontend.cpp
    FrontendConfig.cpp
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
    Instrumentation/Checks/AssertionFailCheck.cpp
    Instrumentation/Checks/DivisionByZeroCheck.cpp
    Instrumentation/Checks/SignedIntegerOverflowCheck.cpp
    Transform/LoopExitCanonizationPass.cpp)

add_library(GazerLLVM SHARED ${SOURCE_FILES})

require_llvm_libraries(GAZER_LLVM_LIBS core irreader transformutils scalaropts ipo)
target_link_libraries(GazerLLVM ${GAZER_LLVM_LIBS} GazerCore GazerTrace GazerAutomaton)
