set(SOURCE_FILES
    Expr.cpp
    LiteralExpr.cpp
    Type.cpp
    Variable.cpp
    SymbolTable.cpp
    Valuation.cpp
    Expr/ExprBuilder.cpp
    Expr/FoldingExprBuilder.cpp
    Expr/ExprPrinter.cpp
    Expr/ExprEvaluator.cpp
)

add_library(GazerCore ${SOURCE_FILES})
target_link_libraries(GazerCore ${LLVM_LIBS})
