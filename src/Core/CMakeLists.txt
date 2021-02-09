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
    Expr/ExprRewrite.cpp
    Expr/ExprUtils.cpp
)

add_library(GazerCore SHARED ${SOURCE_FILES})
target_link_libraries(GazerCore GazerSupport)

set_target_properties(GazerCore PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/lib"
)