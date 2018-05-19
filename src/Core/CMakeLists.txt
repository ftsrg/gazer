set(SOURCE_FILES
    Expr.cpp
    LiteralExpr.cpp
    Type.cpp
    Variable.cpp
    SymbolTable.cpp
    Automaton.cpp
)

add_library(GazerCore ${SOURCE_FILES})
target_link_libraries(GazerCore ${LLVM_LIBS} fmt)
