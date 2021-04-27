set(SOURCE_FILES
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
    Analysis/PDG.cpp
    Instrumentation/Checks/AssertionFailCheck.cpp
    Instrumentation/Checks/DivisionByZeroCheck.cpp
    Instrumentation/Checks/SignedIntegerOverflowCheck.cpp Transform/LoopExitCanonizationPass.cpp)

find_package(LLVM REQUIRED CONFIG)
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
include(AddLLVM)

llvm_map_components_to_libnames(GAZER_LLVM_LIBS core irreader transformutils scalaropts ipo)
message(STATUS "Using LLVM libraries: ${GAZER_LLVM_LIBS}")

add_llvm_library(GazerLLVMBase SHARED ${SOURCE_FILES})
target_link_libraries(GazerLLVMBase GazerCore GazerTrace GazerZ3Solver GazerAutomaton)

# CMake needs a file to link against for a library.
# This is, however, unnecessary when creating a shared lib from an (LLVM) module.
# This is a dirty workaround.
# file(CONFIGURE TOUCH ${DESTPATH}/empty.cpp)
file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp CONTENT \ )

add_library(GazerLLVM SHARED ${CMAKE_CURRENT_BINARY_DIR}/empty.cpp)
target_link_libraries(GazerLLVM GazerLLVMBase ${GAZER_LLVM_LIBS})
