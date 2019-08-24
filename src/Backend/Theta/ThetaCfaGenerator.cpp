#include "gazer/Backend/Theta/ThetaCfaGenerator.h"

#include "gazer/Core/Expr.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprVisitor.h"

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>

#include <variant>

using namespace gazer;

namespace
{

constexpr std::array Keywords = {
    "main", "process", "var", "loc",
    "assume", "init", "final", "error",
    "return", "havoc", "bool", "int", "rat",
    "if", "then", "else", "iff", "imply",
    "forall", "exists", "or", "and", "not",
    "mod", "rem", "true", "false"
};

class ThetaAst
{
public:
    virtual void print(llvm::raw_ostream& os) const = 0;
};

class ThetaLocDecl : public ThetaAst
{
public:
    void print(llvm::raw_ostream& os) const override
    {
        os << "loc " << mName;
    }

private:
    std::string mName;
};

class ThetaStmt : public ThetaAst
{
    using VariantTy = std::variant<ExprPtr, VariableAssignment, Variable*>;

    /* implicit */ ThetaStmt(VariantTy content)
        : mContent(content)
    {}
public:
    static ThetaStmt Assume(ExprPtr expr)
    {
        assert(expr->getType().isBoolType());

        return VariantTy{expr};
    }

    static ThetaStmt Assign(VariableAssignment assign)
    {
        assert(!llvm::isa<UndefExpr>(assign.getValue())
            && "Cannot assign an undef value to a variable."
            "Use ::Havoc() to represent a nondetermistic value assignment."
        );
        return VariantTy{assign};
    }

    static ThetaStmt Havoc(Variable* variable)
    {
        return VariantTy{variable};
    }

    void print(llvm::raw_ostream& os) const override;

private:
    VariantTy mContent;
};

class ThetaEdgeDecl : public ThetaAst
{
public:
    void print(llvm::raw_ostream& os) const override;
private:
    std::string mSource;
    std::string mTarget;
    std::vector<ThetaStmt> mStmts;
};

class ThetaProcess : public ThetaAst
{
public:
    void print(llvm::raw_ostream& os) const override;
private:
    std::string mName;
    std::vector<ThetaLocDecl>  mLocs;
    std::vector<ThetaEdgeDecl> mEdges;
};

} // end anonymous namespace

void ThetaStmt::print(llvm::raw_ostream& os) const
{
    struct PrintVisitor
    {
        llvm::raw_ostream& mOS;
        explicit PrintVisitor(llvm::raw_ostream& os) : mOS(os) {}

        void operator()(const ExprPtr& expr) {
            mOS << "assume ";
            theta::PrintThetaExpr(expr, mOS);
        }

        void operator()(const VariableAssignment& assign) {
            mOS << assign.getVariable() << " := ";
            theta::PrintThetaExpr(assign.getValue(), mOS);
        }

        void operator()(const Variable* variable) {
            mOS << "havoc " << variable->getName();
        }
    } visitor(os);

    std::visit(visitor, mContent);
}

void ThetaEdgeDecl::print(llvm::raw_ostream& os) const
{
    os << mSource << " -> " << mTarget << "{\n";
    for (auto& stmt : mStmts) {
        os << "    ";
        stmt.print(os);
        os << "\n";
    }
    os << "}";
}

void ThetaProcess::print(llvm::raw_ostream& os) const
{
    os << "process " << mName << "{";
    for (auto& loc : mLocs) {
        loc.print(os);
        os << "\n";
    }

    for (auto& edge : mEdges) {
        edge.print(os);
        os << "\n";
    }

    os << "}";
}

namespace
{

class ThetaPrintVisitor : public ExprVisitor<std::string>
{
public:
    std::string visitExpr(const ExprPtr& expr) override {
        return "???";
    }
    
};

};

void theta::PrintThetaExpr(const ExprPtr& expr, llvm::raw_ostream& os)
{

}
