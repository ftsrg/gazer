//==- InstToExpr.h - Translate LLVM IR to expressions -----------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
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
#ifndef GAZER_LLVM_INSTTOEXPR_H
#define GAZER_LLVM_INSTTOEXPR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/LLVM/Memory/ValueOrMemoryObject.h"
#include "gazer/LLVM/Memory/MemoryInstructionHandler.h"
#include "gazer/LLVM/TypeTranslator.h"

#include "gazer/LLVM/LLVMFrontendSettings.h"

#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>

namespace gazer
{

class MemoryModel;

/// A transformation class which may be used to transform LLVM instructions
/// to gazer expressions.
class InstToExpr
{
public:
    InstToExpr(
        llvm::Function& function,
        ExprBuilder& builder,
        LLVMTypeTranslator& types,
        MemoryInstructionHandler& memoryInstHandler,
        const LLVMFrontendSettings& settings
    ) : mFunction(function),
        mExprBuilder(builder),
        mTypes(types),
        mContext(builder.getContext()),
        mMemoryInstHandler(memoryInstHandler),
        mSettings(settings)
    {}

    ExprPtr transform(const llvm::Instruction& inst, Type& expectedType);

    virtual ~InstToExpr() = default;

protected:
    virtual Variable* getVariable(ValueOrMemoryObject value) = 0;

    /// If \p value was inlined, returns the corresponding expression.
    /// Otherwise, this method should return nullptr.
    virtual ExprPtr lookupInlinedVariable(ValueOrMemoryObject value) {
        return nullptr;
    }

    ExprPtr visitBinaryOperator(const llvm::BinaryOperator& binop, Type& expectedType);
    ExprPtr visitSelectInst(const llvm::SelectInst& select, Type& expectedType);
    ExprPtr visitCastInst(const llvm::CastInst& cast, Type& expectedType);
    ExprPtr visitICmpInst(const llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(const llvm::FCmpInst& fcmp);
    ExprPtr visitCallInst(const llvm::CallInst& call);
    ExprPtr visitInsertValueInst(const llvm::InsertValueInst& insert);
    ExprPtr visitExtractValueInst(const llvm::ExtractValueInst& extract);

    ExprPtr operand(ValueOrMemoryObject value);
    ExprPtr operand(ValueOrMemoryObject value, Type& expectedType);
    
    ExprPtr asBool(const ExprPtr& operand);
    ExprPtr asBv(const ExprPtr& operand, unsigned int bits);
    ExprPtr asInt(const ExprPtr& operand);

    ExprPtr integerCast(const llvm::CastInst& cast, const ExprPtr& castOperand, Type& expectedType);
    ExprPtr bitCast(const ExprPtr& castOperand, Type& expectedType);
    ExprPtr castResult(const ExprPtr& expr, const Type& type);
    ExprPtr boolToIntCast(const llvm::CastInst& cast, const ExprPtr& operand, Type& expectedType);

    gazer::Type& translateType(const llvm::Type* type);

    template<class Ty>
    Ty& translateTypeTo(const llvm::Type* type)
    {
        gazer::Type& gazerTy = this->translateType(type);
        assert(llvm::isa<Ty>(&gazerTy));

        return *llvm::cast<Ty>(&gazerTy);
    }

private:
    ExprPtr doTransform(const llvm::Instruction& inst, Type& expectedType);
    ExprPtr unsignedLessThan(const ExprPtr& left, const ExprPtr& right);

    ExprPtr translateICmpBv(llvm::CmpInst::Predicate predicate, const ExprPtr& left, const ExprPtr& right);
    ExprPtr translateICmpInt(llvm::CmpInst::Predicate predicate, const ExprPtr& left, const ExprPtr& right);

    ExprPtr operandValue(const llvm::Value* value);
    ExprPtr operandMemoryObject(const MemoryObjectDef* def);

    ExprPtr handleOverflowPredicate(const llvm::CallInst& call);

    ExprPtr tryToRepresentBitOperator(const llvm::BinaryOperator& binOp, const ExprPtr& left, const ExprPtr& right);

protected:
    llvm::Function& mFunction;
    ExprBuilder& mExprBuilder;
    LLVMTypeTranslator& mTypes;
    GazerContext& mContext;
    MemoryInstructionHandler& mMemoryInstHandler;
    const LLVMFrontendSettings& mSettings;
};

} // end namespace gazer

#endif
