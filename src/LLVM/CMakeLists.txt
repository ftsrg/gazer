set(SOURCE_FILES
    Analysis/CfaBuilderPass.cpp
    Analysis/BmcPass.cpp
    Analysis/ProgramDependence.cpp
    Analysis/TopologicalSort.cpp
    Transform/BoundedUnwindPass.cpp
    Transform/BackwardSlicer.cpp
    Transform/InlineGlobalVariablesPass.cpp
    Transform/SimplifyCFA.cpp
    Transform/CombineErrorCalls.cpp
    Instrumentation/PromoteUndefs.cpp
    Instrumentation/MarkFunctionEntries.cpp
    Instrumentation/InsertLastAddress.cpp
    BMC/BmcTrace.cpp
    BMC/Ir2Expr.cpp
    BMC/BMC.cpp
    TestGenerator/TestGenerator.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS support core irreader transformutils scalaropts ipo)
message(STATUS "Using LLVM libraries: ${LLVM_LIBS}")

add_library(GazerLLVM ${SOURCE_FILES})
target_link_libraries(GazerLLVM ${LLVM_LIBS} GazerCore GazerBMC GazerZ3Solver)
