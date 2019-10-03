#include "ThetaCfaGenerator.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Automaton/CfaTransforms.h"

#include <llvm/ADT/Twine.h>

#include <boost/algorithm/cxx11/any_of.hpp>

#include <variant>

using namespace gazer;
using namespace gazer::theta;

using llvm::dyn_cast;

namespace
{

constexpr std::array ThetaKeywords = {
    "main", "process", "var", "loc",
    "assume", "init", "final", "error",
    "return", "havoc", "bool", "int", "rat",
    "if", "then", "else", "iff", "imply",
    "forall", "exists", "or", "and", "not",
    "mod", "rem", "true", "false"
};

struct ThetaAst
{
    virtual void print(llvm::raw_ostream& os) const = 0;

    virtual ~ThetaAst() = default;
};

struct ThetaLocDecl : ThetaAst
{
    enum Flag
    {
        Loc_State,
        Loc_Init,
        Loc_Final,
        Loc_Error,
    };

    ThetaLocDecl(std::string name, Flag flag = Loc_State)
        : mName(name), mFlag(flag)
    {}

    void print(llvm::raw_ostream& os) const override
    {
        switch (mFlag) {
            case Loc_Init:  os << "init "; break;
            case Loc_Final: os << "final "; break;
            case Loc_Error: os << "error "; break;
            default:
                break;
        }

        os << "loc " << mName;
    }

    std::string mName;
    Flag mFlag;
};

struct ThetaStmt : ThetaAst
{
    using VariantTy = std::variant<ExprPtr, VariableAssignment, Variable*>;

    /* implicit */ ThetaStmt(VariantTy content)
        : mContent(content)
    {}

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

    VariantTy mContent;
};

struct ThetaEdgeDecl : ThetaAst
{
    void print(llvm::raw_ostream& os) const override;

    std::string mSource;
    std::string mTarget;
    std::vector<ThetaStmt> mStmts;
};

class ThetaVarDecl : ThetaAst
{
public:
    ThetaVarDecl(std::string name, std::string type)
        : mName(name), mType(type)
    {}

    llvm::StringRef getName() { return mName; }

    void print(llvm::raw_ostream& os) const override;

private:
    std::string mName;
    std::string mType;
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
            mOS << theta::printThetaExpr(expr);
        }

        void operator()(const VariableAssignment& assign) {
            mOS << assign.getVariable() << " := ";
            mOS << theta::printThetaExpr(assign.getValue());
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

static std::string typeName(Type& type)
{
    switch (type.getTypeID()) {
        case Type::IntTypeID:
            return "int";
        case Type::RealTypeID:
            return "rat";
        case Type::BoolTypeID:
            return "bool";
        default:
            llvm_unreachable("Types which are unsupported by theta should have been eliminated earlier!");
    }
}

void ThetaCfaGenerator::write(llvm::raw_ostream& os)
{
    Cfa* main = mSystem.getMainAutomaton();
    TransformRecursiveToCyclic(main);

    main->view();
#if 0
    // Create a closure to test variable names
    auto isValidVarName = [&vars](llvm::StringRef name) -> bool {
        // The variable name should not be a reserved theta keyword.
        bool valid = !boost::algorithm::any_of_equal(ThetaKeywords, name);

        // The variable name should not be present in the variable list.
        valid &= std::find_if(vars.begin(), vars.end(), [name](auto& v1) {
            return name == v1.getName();
        }) == vars.end();

        return valid;
    };

    // Add variables
    // TODO: We currently ignore inputs and outputs...
    for (auto& variable : root->locals()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

        vars.emplace_back(name, type);
    }
#endif
}

std::string ThetaCfaGenerator::validName(llvm::Twine basename, std::function<bool(llvm::StringRef)> isValid)
{
    llvm::SmallVector<char, 32> buffer;

    llvm::StringRef name = basename.toStringRef(buffer);
    while (!isValid(name)) {
        llvm::Twine nextTry = basename + llvm::Twine(mTmpCount++);
        name = nextTry.toStringRef(buffer);
    }

    return name.str();
}
