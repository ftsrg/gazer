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
        auto mark = module.getOrInsertFunction(
            "gazer.function.entry",
            llvm::Type::getVoidTy(module.getContext()),
            llvm::Type::getMetadataTy(module.getContext())
        );

        IRBuilder<> builder(module.getContext());
        for (Function& function : module) {
            if (function.isDeclaration()) {
                continue;
            }

            BasicBlock& entry = function.getEntryBlock();

            auto dsp = function.getSubprogram();
            if (dsp) {
                builder.SetInsertPoint(&entry, entry.getFirstInsertionPt());
                builder.CreateCall(mark, {
                    MetadataAsValue::get(module.getContext(), dsp)
                });
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