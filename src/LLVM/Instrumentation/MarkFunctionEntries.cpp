#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>

#include <llvm/Pass.h>

using namespace gazer;
using namespace llvm;

namespace
{

class MarkFunctionEntriesPass : public ModulePass
{
public:
    static char ID;

    MarkFunctionEntriesPass()
        : ModulePass(ID)
    {}

    bool runOnModule(Module& module) override
    {
        LLVMContext& context = module.getContext();
        llvm::DenseMap<llvm::Type*, llvm::Value*> returnValueMarks;
       
        auto mark = GazerIntrinsic::GetOrInsertFunctionEntry(module);
        auto retMarkVoid = GazerIntrinsic::GetOrInsertFunctionReturnVoid(module);        
        auto callReturnedMark = GazerIntrinsic::GetOrInsertFunctionCallReturned(module);

        IRBuilder<> builder(context);
        for (Function& function : module) {
            if (function.isDeclaration()) {
                continue;
            }

            BasicBlock& entry = function.getEntryBlock();

            auto dsp = function.getSubprogram();
            if (dsp != nullptr) {
                builder.SetInsertPoint(&entry, entry.getFirstInsertionPt());
                builder.CreateCall(mark, {
                    MetadataAsValue::get(context, dsp),
                    builder.getInt8(function.arg_size())
                });
            } else {
                llvm::errs()
                    << "Cannot insert function entry marks: "
                    << "DISubprogram missing.\n";
                continue;
            }

            // Also mark call returns to other functions
            std::vector<ReturnInst*> returns;
            for (Instruction& inst : llvm::instructions(function)) {
                if (auto ret = dyn_cast<ReturnInst>(&inst)) {
                    returns.push_back(ret);
                }
            }

            for (ReturnInst* ret : returns) {
                builder.SetInsertPoint(ret);
                llvm::Value* retValue = ret->getReturnValue();
                if (retValue != nullptr) {
                    auto retValueTy = retValue->getType();
                    llvm::Value* retMark = returnValueMarks[retValueTy];
                    if (retMark == nullptr) {
                        std::string nameBuffer;
                        llvm::raw_string_ostream rso(nameBuffer);
                        retValueTy->print(rso, false, true);
                        rso.flush();

                        // Insert a new function for this mark type
                        retMark = GazerIntrinsic::GetOrInsertFunctionReturnValue(module, retValueTy).getCallee();
                        returnValueMarks[retValueTy] = retMark;
                    }

                    builder.CreateCall(retMark, {
                        MetadataAsValue::get(context, dsp),
                        retValue
                    });
                } else {
                    builder.CreateCall(retMarkVoid, {
                        MetadataAsValue::get(context, dsp)
                    });
                }
            }

            // Also mark call returns from other functions
            std::vector<CallInst*> calls;
            for (Instruction& inst : llvm::instructions(function)) {
                if (auto call = dyn_cast<CallInst>(&inst)) {
                    Function* callee = call->getCalledFunction();
                    if (callee == nullptr || callee->isDeclaration()) {
                        // Currently we do not bother with indirect calls,
                        // also we only need functions which have a definition.
                        continue;
                    }

                    calls.push_back(call);
                }
            }

            for (CallInst* call : calls) {
                builder.SetInsertPoint(call->getNextNode());
                builder.CreateCall(callReturnedMark, {
                    MetadataAsValue::get(context, dsp)
                });
            }
        }

        return true;
    }

};

} // end anonymous namespace

char MarkFunctionEntriesPass::ID = 0;

namespace gazer {
    llvm::Pass* createMarkFunctionEntriesPass() {
        return new MarkFunctionEntriesPass();
    }
} // end namespace gazer