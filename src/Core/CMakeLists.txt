set(SOURCE_FILES
    Expr.cpp
    LiteralExpr.cpp
    Type.cpp
    Variable.cpp
    SymbolTable.cpp
    Valuation.cpp
    Utils/ExprBuilder.cpp
    Utils/FoldingExprBuilder.cpp
    Utils/ExprPrinter.cpp
)

add_library(GazerCore ${SOURCE_FILES})
target_link_libraries(GazerCore ${LLVM_LIBS} fmt)
