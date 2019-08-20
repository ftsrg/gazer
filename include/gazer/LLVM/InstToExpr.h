#ifndef GAZER_LLVM_INSTTOEXPR_H
#define GAZER_LLVM_INSTTOEXPR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/LLVM/Analysis/MemoryObject.h"

#include <llvm/IR/Operator.h>
#include <llvm/IR/Instructions.h>

namespace gazer
{

/// A transformation class which may be used to transform LLVM instructions
/// to gazer expressions.
class InstToExpr
{
public:
    InstToExpr(ExprBuilder& builder, MemoryModel& memoryModel)
        : mExprBuilder(builder), mContext(builder.getContext()), mMemoryModel(memoryModel)
    {}

    ExprPtr transform(llvm::Instruction& inst);

    virtual ~InstToExpr() = default;

protected:
    virtual Variable* getVariable(const llvm::Value* value) = 0;

    /// If \p value was inlined, returns the corresponding expressions.
    /// Otherwise, this method should return nullptr.
    virtual ExprPtr lookupInlinedVariable(const llvm::Value* value) {
        return nullptr;
    }

protected:
    ExprPtr visitBinaryOperator(llvm::BinaryOperator& binop);
    ExprPtr visitSelectInst(llvm::SelectInst& select);
    ExprPtr visitICmpInst(llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(llvm::FCmpInst& fcmp);
    ExprPtr visitCastInst(llvm::CastInst& cast);
    ExprPtr visitCallInst(llvm::CallInst& call);
    ExprPtr visitLoadInst(llvm::LoadInst& load);
    ExprPtr visitGEPOperator(llvm::GEPOperator& gep);

    ExprPtr operand(const llvm::Value* value);
    
    ExprPtr asBool(const ExprPtr& operand);
    ExprPtr asInt(const ExprPtr& operand, unsigned int bits);

    ExprPtr integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned int width);
    ExprPtr castResult(const ExprPtr& expr, const Type& type);

    gazer::Type& translateType(const llvm::Type* type);

    template<class Ty>
    Ty& translateTypeTo(const llvm::Type* type)
    {
        gazer::Type& gazerTy = this->translateType(type);
        assert(llvm::isa<Ty>(&gazerTy));

        return *llvm::cast<Ty>(&gazerTy);
    }

protected:
    ExprBuilder& mExprBuilder;
    GazerContext& mContext;
    MemoryModel& mMemoryModel;
};

} // end namespace gazer

#endif
