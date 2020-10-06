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
#ifndef GAZER_THETACOMMON_H
#define GAZER_THETACOMMON_H

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/Twine.h>

#include <regex>
#include <variant>

namespace gazer::theta {

std::string printThetaExpr(const ExprPtr& expr);

std::string printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames);

std::string validName(std::string name, std::function<bool(const std::string&)> isUnique);

std::string typeName(Type& type);

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
    struct CallData {
        std::string resultVar;
        std::string functionName;
        std::vector<std::string> parameters;
    };

    struct LoadData {
        std::string globalVariableName;
        std::string valueVariable;
        VariableAssignment::Ordering ordering;
    };
    struct StoreData {
        std::string globalVariableName;
        std::string valueVariable;
        VariableAssignment::Ordering ordering;
    };
    struct FenceData {
        std::string fencyName;
    };
    using VariantTy = std::variant<
        ExprPtr, // assume
        std::pair<std::string, ExprPtr>, // assign
        std::string, // havoc
        CallData, // call
        StoreData, // store
        LoadData, // load
        FenceData
    >;

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

    static ThetaStmt Load(std::string ptr, std::string value,
                          VariableAssignment::Ordering ordering)
    {
        return VariantTy{LoadData{ptr, value, ordering}};
    }

    static ThetaStmt Store(std::string ptr, std::string value,
                          VariableAssignment::Ordering ordering)
    {
        return VariantTy{StoreData{ptr, value, ordering}};
    }

    static ThetaStmt Fence(FenceData fence)
    {
        return VariantTy{fence};
    }

    static ThetaStmt Havoc(std::string variable)
    {
        return VariantTy{variable};
    }

    static ThetaStmt Call(CallData callData)
    {
        return VariantTy{callData};
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
    llvm::StringRef getType() { return mType; }

    void print(llvm::raw_ostream& os) const override
    {
        os << "var " << mName << " : " << mType;
    }

private:
    std::string mName;
    std::string mType;
};

class PrintVisitor
{
    llvm::raw_ostream& mOS;
    std::function<std::string(Variable*)> mCanonizeName;

    void printExpr(const ExprPtr& expr) {
        if (mCanonizeName == nullptr) {
            mOS << printThetaExpr(expr);
        } else {
            mOS << printThetaExpr(expr, mCanonizeName);
        }
    }

    static std::string printOrdering(VariableAssignment::Ordering ordering) {
        switch(ordering) {
        case VariableAssignment::Acquire:
            return "acq";
        case VariableAssignment::AcquireRelease:
            return "acq_rel";
        case VariableAssignment::Release:
            return "rel";
        case VariableAssignment::SequentiallyConsistent:
            return "seq_cst";
        case VariableAssignment::Monotonic:
            return "mon";
        case VariableAssignment::Unordered:
            return "unordered";
        default:
            llvm_unreachable("Invalid memory ordering!");
        }
    }

public:
    PrintVisitor(llvm::raw_ostream& os, std::function<std::string(Variable*)> canonizeName)
        : mOS(os), mCanonizeName(canonizeName)
    {}

    void operator()(const ExprPtr& expr) {
        mOS << "assume ";
        printExpr(expr);
    }

    void operator()(const std::pair<std::string, ExprPtr>& assign) {
        mOS << assign.first << " := ";
        printExpr(assign.second);
    }

    void operator()(const ThetaStmt::StoreData& store) {
        assert(store.ordering != VariableAssignment::Ordering::NotSpecified);
        mOS << store.globalVariableName << " <- " << store.valueVariable;
        if (store.ordering != VariableAssignment::Ordering::NotAtomic) {
            mOS << " atomic @ " << printOrdering(store.ordering);
        }
    }

    void operator()(const ThetaStmt::LoadData& load) {
        assert(load.ordering != VariableAssignment::Ordering::NotSpecified);
        mOS << load.globalVariableName << " -> " << load.valueVariable;
        if (load.ordering != VariableAssignment::Ordering::NotAtomic) {
            mOS << " atomic @ " << printOrdering(load.ordering);
        }
    }

    void operator()(const ThetaStmt::FenceData& fence) {
        mOS << "fence " << fence.fencyName;
    }

    void operator()(const std::string& variable) {
        mOS << "havoc " << variable;
    }

    void operator()(const ThetaStmt::CallData& function) {
        if (function.resultVar != "") {
            mOS << function.resultVar << " := ";
        }
        mOS << "call " << function.functionName << "(";
        bool first = true;
        for (const std::string& param : function.parameters) {
            if (!first) {
                mOS << ", ";
            } else {
                first = false;
            }
            mOS << param;
        }
        mOS << ")";
    }
};

std::string thetaName(gazer::Cfa*);

} // namespace gazer::theta

#endif // GAZER_THETACOMMON_H
