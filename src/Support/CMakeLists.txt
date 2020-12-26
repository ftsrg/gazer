set(SOURCE_FILES
    SExpr.cpp
    Runtime.cpp)

require_llvm_libraries(GAZER_LLVM_LIBS support)

add_library(GazerSupport ${SOURCE_FILES})
target_link_libraries(GazerSupport ${GAZER_LLVM_LIBS})
