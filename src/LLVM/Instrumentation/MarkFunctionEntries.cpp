#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>

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
        llvm::DenseMap<llvm::Type*, llvm::Constant*> returnValueMarks;
       
        // void gazer.function.entry(metadata function_name, i8 num_args);
        auto mark = module.getOrInsertFunction(
            "gazer.function.entry",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context),
            llvm::Type::getInt8Ty(context)
        );

        // void gazer.function.return(metadata function_name, metadata return_value);
        /*auto retMark = module.getOrInsertFunction(
            "gazer.function.return",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context),
            llvm::Type::getMetadataTy(context)
        ); */

        auto retMarkVoid = module.getOrInsertFunction(
            "gazer.function.return_void",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context)
        );
        auto callReturnedMark = module.getOrInsertFunction(
            "gazer.function.call_returned",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context)
        );
    
        auto argMark = module.getOrInsertFunction(
            "gazer.function.arg",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context),
            llvm::Type::getInt8Ty(context)
        );
        auto argEnd = module.getOrInsertFunction(
            "gazer.function.arg_end",
            llvm::Type::getVoidTy(context)
        );

        IRBuilder<> builder(context);
        for (Function& function : module) {
            if (function.isDeclaration()) {
                continue;
            }

            BasicBlock& entry = function.getEntryBlock();

            auto dsp = function.getSubprogram();
            if (dsp) {
                builder.SetInsertPoint(&entry, entry.getFirstInsertionPt());
                builder.CreateCall(mark, {
                    MetadataAsValue::get(context, dsp),
                    builder.getInt8(function.arg_size())
                });

                size_t argCnt = 0;
                for (llvm::Argument& argument : function.args()) {
                    builder.CreateCall(argMark, {
                        MetadataAsValue::get(context, ValueAsMetadata::get(&argument)),
                        builder.getInt8(argCnt++)
                    });
                }
                builder.CreateCall(argEnd);
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
                auto retValueTy = ret->getReturnValue()->getType();
                if (!retValueTy->isVoidTy()) {
                    llvm::Constant* retMark = returnValueMarks[retValueTy];
                    if (retMark == nullptr) {
                        std::string nameBuffer;
                        llvm::raw_string_ostream rso(nameBuffer);
                        retValueTy->print(rso, false, true);
                        rso.flush();

                        // Insert a new function for this mark type
                        retMark = module.getOrInsertFunction(
                            "gazer.function.return_value." + rso.str(),
                            llvm::Type::getVoidTy(context),
                            llvm::Type::getMetadataTy(context),
                            retValueTy
                        );
                        returnValueMarks[retValueTy] = retMark;
                    }

                    auto md = ValueAsMetadata::get(ret->getReturnValue());
                    builder.CreateCall(retMark, {
                        MetadataAsValue::get(context, dsp),
                        ret->getReturnValue()
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
                    if (callee == nullptr) {
                        // Currently we do not bother with indirect calls.
                        continue;
                    } else if (callee->isDeclaration()) {
                        // We are only interested in functions for which we can find a body.
                        continue;
                    }

                    calls.push_back(call);
                }
            }

            for (CallInst* call : calls) {
                auto md = ValueAsMetadata::get(call);
                builder.SetInsertPoint(call->getNextNode());
                builder.CreateCall(callReturnedMark, {
                    MetadataAsValue::get(context, dsp)
                });
            }
        }

        return true;
    }

};

}

char MarkFunctionEntriesPass::ID = 0;

namespace gazer {
    llvm::Pass* createMarkFunctionEntriesPass() {
        return new MarkFunctionEntriesPass();
    }
}