set(SOURCE_FILES
    SExpr.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS support)

add_library(GazerSupport ${SOURCE_FILES})
target_link_libraries(GazerSupport ${LLVM_LIBS})
