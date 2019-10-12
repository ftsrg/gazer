#include "ThetaCfaGenerator.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Automaton/CfaTransforms.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>

#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/join.hpp>

#include <regex>
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
    using VariantTy = std::variant<ExprPtr, std::pair<std::string, ExprPtr>, std::string>;

    /* implicit */ ThetaStmt(VariantTy content)
        : mContent(content)
    {}

    static ThetaStmt Assume(ExprPtr expr)
    {
        assert(expr->getType().isBoolType());

        return VariantTy{expr};
    }

    static ThetaStmt Assign(std::string variableName, ExprPtr value)
    {
        assert(!llvm::isa<UndefExpr>(value)
            && "Cannot assign an undef value to a variable."
            "Use ::Havoc() to represent a nondetermistic value assignment."
        );

        std::pair<std::string, ExprPtr> pair = { variableName, value };

        return VariantTy{pair};
    }

    static ThetaStmt Havoc(std::string variable)
    {
        return VariantTy{variable};
    }

    void print(llvm::raw_ostream& os) const override;

    VariantTy mContent;
};

struct ThetaEdgeDecl : ThetaAst
{
    ThetaEdgeDecl(ThetaLocDecl& source, ThetaLocDecl& target, std::vector<ThetaStmt> stmts)
        : mSource(source), mTarget(target), mStmts(std::move(stmts))
    {}

    void print(llvm::raw_ostream& os) const override;

    ThetaLocDecl& mSource;
    ThetaLocDecl& mTarget;
    std::vector<ThetaStmt> mStmts;
};

class ThetaVarDecl : ThetaAst
{
public:
    ThetaVarDecl(std::string name, std::string type)
        : mName(name), mType(type)
    {}

    llvm::StringRef getName() { return mName; }

    void print(llvm::raw_ostream& os) const override
    {
        os << "var " << mName << " : " << mType;
    }

private:
    std::string mName;
    std::string mType;
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

        void operator()(const std::pair<std::string, ExprPtr>& assign) {
            mOS << assign.first << " := ";
            mOS << theta::printThetaExpr(assign.second);
        }

        void operator()(const std::string& variable) {
            mOS << "havoc " << variable;
        }
    } visitor(os);

    std::visit(visitor, mContent);
}

void ThetaEdgeDecl::print(llvm::raw_ostream& os) const
{
    os << mSource.mName << " -> " << mTarget.mName << " {\n";
    for (auto& stmt : mStmts) {
        os << "    ";
        stmt.print(os);
        os << "\n";
    }
    os << "}\n";
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

void ThetaCfaGenerator::write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace)
{
    //for (auto& cfa : mSystem) {
    //    cfa.view();
    //}

    Cfa* main = mSystem.getMainAutomaton();
    auto recursiveToCyclicResult = TransformRecursiveToCyclic(main);

    nameTrace.errorLocation = recursiveToCyclicResult.errorLocation;
    nameTrace.errorFieldVariable = recursiveToCyclicResult.errorFieldVariable;
    nameTrace.inlinedLocations = std::move(recursiveToCyclicResult.inlinedLocations);
    nameTrace.inlinedVariables = std::move(recursiveToCyclicResult.inlinedVariables);

    llvm::DenseMap<Location*, std::unique_ptr<ThetaLocDecl>> locs;
    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>> vars;
    std::vector<std::unique_ptr<ThetaEdgeDecl>> edges;

    // Create a closure to test variable names
    auto isValidVarName = [&vars](const std::string& name) -> bool {
        // The variable name should not be present in the variable list.
        return std::find_if(vars.begin(), vars.end(), [name](auto& v1) {
            return name == v1.second->getName();
        }) == vars.end();
    };

    // Add variables
    for (auto& variable : main->locals()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());
        
        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    for (auto& variable : main->inputs()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    // Add locations
    for (auto& loc : main->nodes()) {
        ThetaLocDecl::Flag flag = ThetaLocDecl::Loc_State;
        if (&*loc == recursiveToCyclicResult.errorLocation) {
            flag = ThetaLocDecl::Loc_Error;
        } else if (main->getEntry() == &*loc) {
            flag = ThetaLocDecl::Loc_Init;
        } else if (main->getExit() == &*loc) {
            flag = ThetaLocDecl::Loc_Final;
        }

        auto locName = "loc" + std::to_string(loc->getId());

        nameTrace.locations[locName] = &*loc;
        locs.try_emplace(&*loc, std::make_unique<ThetaLocDecl>(locName, flag));
    }

    // Add edges
    for (auto& edge : main->edges()) {
        ThetaLocDecl& source = *locs[edge->getSource()];
        ThetaLocDecl& target = *locs[edge->getTarget()];
        
        std::vector<ThetaStmt> stmts;

        if (edge->getGuard() != BoolLiteralExpr::True(edge->getGuard()->getContext())) {
            stmts.push_back(ThetaStmt::Assume(edge->getGuard()));
        }

        if (auto assignEdge = dyn_cast<AssignTransition>(&*edge)) {
            for (auto& assignment : *assignEdge) {
                auto lhsName = vars[assignment.getVariable()]->getName();

                if (llvm::isa<UndefExpr>(assignment.getValue())) {
                    stmts.push_back(ThetaStmt::Havoc(lhsName));
                } else {
                    stmts.push_back(ThetaStmt::Assign(lhsName, assignment.getValue()));
                }
            }
        } else if (auto callEdge = dyn_cast<CallTransition>(&*edge)) {
            llvm_unreachable("CallTransitions are not supported in theta CFAs!");
        }

        edges.emplace_back(std::make_unique<ThetaEdgeDecl>(source, target, std::move(stmts)));
    }

    auto INDENT  = "    ";
    auto INDENT2 = "        ";

    auto canonizeName = [&vars](Variable* variable) -> std::string {
        if (vars.count(variable) == 0) {
            return variable->getName();
        }

        return vars[variable]->getName();
    };

    os
        << "main process __gazer_main_process {\n";
        //<< INDENT  << "main procedure __gazer_main_entry {\n";
    
    for (auto& varEntry : vars) {
        os << INDENT;
        varEntry.second->print(os);
        os << "\n";
    }

    for (auto& locEntry : locs) {
        os << INDENT;
        locEntry.second->print(os);
        os << "\n";
    }

    for (auto& edge : edges) {
        os << INDENT << edge->mSource.mName << " -> " << edge->mTarget.mName << " {\n";
        for (auto& stmt : edge->mStmts) {
            os << INDENT2;
            struct PrintVisitor
            {
                llvm::raw_ostream& mOS;
                std::function<std::string(Variable*)> mCanonizeName;

                PrintVisitor(llvm::raw_ostream& os, std::function<std::string(Variable*)> canonizeName)
                    : mOS(os), mCanonizeName(canonizeName)
                {}

                void operator()(const ExprPtr& expr) {
                    mOS << "assume ";
                    mOS << theta::printThetaExpr(expr, mCanonizeName);
                }

                void operator()(const std::pair<std::string, ExprPtr>& assign) {
                    mOS << assign.first << " := ";
                    mOS << theta::printThetaExpr(assign.second, mCanonizeName);
                }

                void operator()(const std::string& variable) {
                    mOS << "havoc " << variable;
                }
            } visitor(os, canonizeName);

            std::visit(visitor, stmt.mContent);
            os << "\n";
        }
        os << INDENT << "}\n";
        os << "\n";
    }

    os << "}\n";
    os.flush();
}

std::string ThetaCfaGenerator::validName(std::string name, std::function<bool(const std::string&)> isUnique)
{
    name = std::regex_replace(name, std::regex("[^a-zA-Z0-9_]"), "_");

    if (std::find(ThetaKeywords.begin(), ThetaKeywords.end(), name) != ThetaKeywords.end()) {
        name += "_gazer";
    }

    while (!isUnique(name)) {
        llvm::Twine nextTry = name + llvm::Twine(mTmpCount++);
        name = nextTry.str();
    }

    return name;
}
