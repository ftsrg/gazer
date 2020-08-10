//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_THETAUTIL_H
#define GAZER_THETAUTIL_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/LiteralExpr.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Twine.h>
#include <string>
#include <variant>

namespace gazer::theta {

std::string printThetaExpr(const ExprPtr& expr);

std::string printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames);

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
    using callType = std::tuple<std::string, // callee name
    llvm::SmallVector<std::string, 5>, // variables
    std::optional<std::string>>; // return variable
    using VariantTy = std::variant<gazer::ExprPtr, std::pair<std::string, gazer::ExprPtr>, std::string, callType>;

    /* implicit */ ThetaStmt(VariantTy content)
        : mContent(content)
    {}

    static ThetaStmt Assume(gazer::ExprPtr expr)
    {
        assert(expr->getType().isBoolType());

        return VariantTy{expr};
    }

    static ThetaStmt Assign(std::string variableName, gazer::ExprPtr value)
    {
        assert(!llvm::isa<gazer::UndefExpr>(value)
               && "Cannot assign an undef value to a variable."
                  "Use ::Havoc() to represent a nondetermistic value assignment."
        );

        std::pair<std::string, gazer::ExprPtr> pair = { variableName, value };

        return VariantTy{pair};
    }

    static ThetaStmt Havoc(std::string variable)
    {
        return VariantTy{variable};
    }

    void print(llvm::raw_ostream& os) const override;

    VariantTy mContent;
    static ThetaStmt Call(
        llvm::StringRef name,
        llvm::SmallVector<std::string, 5> vector,
        std::optional<std::string> result) {
        return VariantTy{callType{name, vector, result}};
    }
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

    llvm::StringRef getType() { return mType; }

    void print(llvm::raw_ostream& os) const override
    {
        os << "var " << mName << " : " << mType;
    }

private:
    std::string mName;
    std::string mType;
};

struct PrintVisitor
{
    llvm::raw_ostream& mOS;
    std::function<std::string(gazer::Variable*)> mCanonizeName;

    PrintVisitor(llvm::raw_ostream& os, std::function<std::string(gazer::Variable*)> canonizeName)
        : mOS(os), mCanonizeName(std::move(canonizeName))
    {}

    explicit PrintVisitor(llvm::raw_ostream& os)
        : mOS(os), mCanonizeName([](gazer::Variable* v) {return v->getName();})
    {}

    void operator()(const gazer::ExprPtr& expr) {
        mOS << "assume ";
        mOS << printThetaExpr(expr, mCanonizeName);
    }

    void operator()(const std::pair<std::string, gazer::ExprPtr>& assign) {
        mOS << assign.first << " := ";
        mOS << printThetaExpr(assign.second, mCanonizeName);
    }

    void operator()(const std::string& variable) {
        mOS << "havoc " << variable;
    }

    void operator()(const gazer::theta::ThetaStmt::callType& call) {
        auto result = std::get<2>(call);
        auto prefix = result.has_value() ? result.value() + llvm::Twine(" := ") : "";

        // TODO main -> xmain temp solution
        mOS << prefix << "call x" << std::get<0>(call) << "(";
        bool first = true;
        for (const auto& param : std::get<1>(call)) {
            if (first) {
                first = false;
            } else {
                mOS << ", ";
            }
            mOS << param;
        }
        mOS << ")";
    }
};

std::string validName(std::string name, std::function<bool(const std::string&)> isUnique);

static std::string typeName(Type& type)
{
    switch (type.getTypeID()) {
    case Type::IntTypeID:
        return "int";
    case Type::RealTypeID:
        return "rat";
    case Type::BoolTypeID:
        return "bool";
    case Type::ArrayTypeID: {
        auto& arrTy = llvm::cast<ArrayType>(type);
        return "[" + typeName(arrTy.getIndexType()) + "] -> " + typeName(arrTy.getElementType());
    }
    default:
        llvm_unreachable("Types which are unsupported by theta should have been eliminated earlier!");
    }
}

struct ThetaNameMapping
{
    llvm::StringMap<Location*> locations;
    llvm::StringMap<Variable*> variables;
    Location* errorLocation;
    Variable* errorFieldVariable;
    llvm::DenseMap<Location*, Location*> inlinedLocations;
    llvm::DenseMap<Variable*, Variable*> inlinedVariables;
};

} // namespace gazer::theta

#endif // GAZER_THETAUTIL_H
