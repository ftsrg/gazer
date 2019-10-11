#ifndef GAZER_LLVM_LLVMTRACEBUILDER_H
#define GAZER_LLVM_LLVMTRACEBUILDER_H

#include "gazer/Trace/Trace.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Value.h>

namespace gazer
{

class CfaToLLVMTrace;

class LLVMTraceBuilder : public CfaTraceBuilder
{
public:
    LLVMTraceBuilder(GazerContext& context, CfaToLLVMTrace& cfaToLlvmTrace)
        : mContext(context), mCfaToLlvmTrace(cfaToLlvmTrace)
    {}

    std::unique_ptr<Trace> build(
        std::vector<Location*>& states,
        std::vector<std::vector<VariableAssignment>>& actions
    ) override;

private:
    ExprRef<AtomicExpr> getLiteralFromValue(Cfa* cfa, const llvm::Value* value, Valuation& model);

private:
    GazerContext& mContext;
    CfaToLLVMTrace& mCfaToLlvmTrace;
};

}

#endif
