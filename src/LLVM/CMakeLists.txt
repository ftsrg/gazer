set(SOURCE_FILES
    Analysis/CfaBuilderPass.cpp
    Transform/BoundedUnwindPass.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS core irreader transformutils scalaropts ipo)

add_library(GazerLLVM ${SOURCE_FILES})
target_link_libraries(GazerLLVM ${LLVM_LIBS} GazerCore)
