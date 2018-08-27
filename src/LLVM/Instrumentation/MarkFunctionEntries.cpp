#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>

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
       
        auto mark = module.getOrInsertFunction(
            "gazer.function.entry",
            llvm::Type::getVoidTy(context),
            llvm::Type::getMetadataTy(context),
            llvm::Type::getInt8Ty(context)
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