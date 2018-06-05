#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

namespace
{

struct InlineGlobalVariablesPass final : public ModulePass
{
    static char ID;

    InlineGlobalVariablesPass()
        : ModulePass(ID)
    {}

    virtual bool runOnModule(Module& module) override;
};

char InlineGlobalVariablesPass::ID = 0;

bool InlineGlobalVariablesPass::runOnModule(Module& module)
{
    Function* main = module.getFunction("main");
    if (main == nullptr) {
        // No main function found or not all functions were inlined.
        return false;
    }

    if (module.global_begin() == module.global_end()) {
        // No globals to inline
        return false;
    }

    IRBuilder<> builder(module.getContext());
    builder.SetInsertPoint(&main->getEntryBlock(), main->getEntryBlock().begin());
    auto gvIt = module.global_begin();
    while (gvIt != module.global_end()) {
        GlobalVariable& gv = *(gvIt++);

        auto type = gv.getType()->getElementType();
        AllocaInst* alloc = builder.CreateAlloca(type, nullptr, gv.getName());
        Value* init = gv.hasInitializer() ? gv.getInitializer() : UndefValue::get(type);
        builder.CreateStore(init, alloc);

        gv.replaceAllUsesWith(alloc);
        gv.eraseFromParent();
    }

    return true;
}

} // end anonymous namespace

namespace gazer
{

llvm::Pass* createInlineGlobalVariablesPass() {
    return new InlineGlobalVariablesPass();
}

}