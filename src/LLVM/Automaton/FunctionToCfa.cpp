#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/CodeExtractor.h>

#include "FunctionToCfa.h"

using namespace gazer;
using namespace llvm;

void FunctionToCfa::encode(Cfa* cfa)
{
    // Insert all arguments into the CFA
    for (auto& argument : mFunction.args()) {
        auto variable = cfa->createInput(
            argument.getName(),
            typeFromLLVMType(argument.getType(), mContext)
        );
        mVariables[&argument] = variable;
    }

    // Add a return value if needed
    if (!mFunction.getReturnType()->isVoidTy()) {
        //cfa->addOutput("RET_VAL");
    }

    // As loops will be represented as separate automata,
    // we need to find and filter out all blocks and instructions
    // which belong to a loop.
    llvm::SmallVector<llvm::Loop*, 4> loopsPreorder = mLoopInfo.getLoopsInPreorder();
    llvm::DenseSet<llvm::BasicBlock*> visitedBlocks;

    while (!loopsPreorder.empty()) {
        llvm::Loop* loop = loopsPreorder.pop_back_val();
        Cfa* loopCfa = mSystem->createNestedCfa(cfa, loop->getName());

        std::unordered_map<llvm::BasicBlock*, std::pair<Location*, Location*>> locations;

        llvm::CodeExtractor extractor(loop->getBlocks());

        llvm::SetVector<Value*> inputs;
        llvm::SetVector<Value*> outputs;
        llvm::SetVector<Value*> allocas;
        extractor.findInputsOutputs(inputs, outputs, allocas);


        for (llvm::BasicBlock* bb : loop->getBlocks()) {
            if (visitedBlocks.count(bb) != 0) {
                continue;
            }

            auto entry = cfa->createLocation();
            auto exit = cfa->createLocation();

            locations[bb] = std::make_pair(entry, exit);
            visitedBlocks.insert(bb);
        }

    }

    // Add local values
    for (auto& instr : llvm::instructions(mFunction)) {
        if (instr.getName() != "") {
            Variable* variable = cfa->createLocal(
                instr.getName(),
                typeFromLLVMType(instr.getType(), mContext)
            );
            mVariables[&instr] = variable;
        }
    }

    // Create new locations for each basic block
    for (llvm::BasicBlock& bb : mFunction) {
        Location* bbEntry = cfa->createLocation();
        Location* bbExit  = cfa->createLocation();

        mBlockMap.try_emplace(&bb, std::make_pair(bbEntry, bbExit));
    }


}

gazer::Type& gazer::typeFromLLVMType(const llvm::Type* type, GazerContext& context)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return BoolType::Get(context);
        }

        //if (UseMathInt && width <= 64) {
        //    return IntType::Get(mContext, width);
        //}

        return BvType::Get(context, width);
    } else if (type->isHalfTy()) {
        return FloatType::Get(context, FloatType::Half);
    } else if (type->isFloatTy()) {
        return FloatType::Get(context, FloatType::Single);
    } else if (type->isDoubleTy()) {
        return FloatType::Get(context, FloatType::Double);
    } else if (type->isFP128Ty()) {
        return FloatType::Get(context, FloatType::Quad);
    } else if (type->isPointerTy()) {
        //return mMemoryModel.getTypeFromPointerType(llvm::cast<llvm::PointerType>(type));
    }

    assert(false && "Unsupported LLVM type.");
}

GenerationContext::GenerationContext(AutomataSystem& system) : System(system)
{}
