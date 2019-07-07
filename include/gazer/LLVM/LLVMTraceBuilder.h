#ifndef GAZER_LLVM_LLVMTRACEBUILDER_H
#define GAZER_LLVM_LLVMTRACEBUILDER_H

#include "gazer/Trace/Trace.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Value.h>

namespace gazer
{

class LLVMTraceBuilderImpl
{
    template<class State>
    friend class LLVMTraceBuilder;
private:
    LLVMTraceBuilderImpl(
        GazerContext& context,
        const llvm::DenseMap<llvm::Value*, Variable*>& values
    ) : mContext(context), mValueMap(values)
    {}

protected:
    std::vector<std::unique_ptr<TraceEvent>> buildEventsFromBlocks(
        Valuation& model, const std::vector<llvm::BasicBlock*> blocks
    );

private:
    ExprRef<AtomicExpr> getLiteralFromValue(llvm::Value* value, Valuation& model);

private:
    GazerContext& mContext;
    const llvm::DenseMap<llvm::Value*, Variable*>& mValueMap;
};

template<class State>
class LLVMTraceBuilder final : public TraceBuilder<State>, LLVMTraceBuilderImpl
{
public:
    LLVMTraceBuilder(
        GazerContext& context,
        std::function<llvm::BasicBlock*(State)> stateToBlock,
        const llvm::DenseMap<llvm::Value*, Variable*>& values
    ) : LLVMTraceBuilderImpl(context, values), mStateToBlock(stateToBlock)
    {}

    LLVMTraceBuilder(const LLVMTraceBuilder&) = delete;
    LLVMTraceBuilder& operator=(const LLVMTraceBuilder&) = delete;

protected:
    std::vector<std::unique_ptr<TraceEvent>> buildEvents(
        Valuation& model,
        const std::vector<State>& states
    ) override {
        std::vector<llvm::BasicBlock*> blocks;
        blocks.reserve(states.size());

        for (const State& state : states) {
            llvm::BasicBlock* result = mStateToBlock(state);
            assert(result != nullptr && "Each state should map to a BasicBlock!");

            blocks.push_back(result);
        }

        return this->buildEventsFromBlocks(model, blocks);
    }
private:
    std::function<llvm::BasicBlock*(State)> mStateToBlock;
};

}

#endif
