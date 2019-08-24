set(SOURCE_FILES
    Expr.cpp
    LiteralExpr.cpp
    Type.cpp
    Variable.cpp
    Valuation.cpp
    GazerContext.cpp
    Expr/ExprBuilder.cpp
    Expr/FoldingExprBuilder.cpp
    Expr/ExprPrinter.cpp
    Expr/ExprEvaluator.cpp
    Expr/ConstantFolder.cpp
    Expr/ExprRewrite.cpp
    Expr/ExprUtils.cpp    
)

add_library(GazerCore ${SOURCE_FILES})
target_link_libraries(GazerCore ${LLVM_LIBS})
