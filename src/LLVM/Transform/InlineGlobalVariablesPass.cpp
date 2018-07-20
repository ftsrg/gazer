#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/DIBuilder.h>

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

    llvm::Constant* mark = module.getOrInsertFunction(
        "gazer.inlined_global.write",
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );

    Function* dbgDecl = Intrinsic::getDeclaration(&module, Intrinsic::dbg_declare);

    IRBuilder<> builder(module.getContext());
    builder.SetInsertPoint(&main->getEntryBlock(), main->getEntryBlock().begin());

    DIBuilder diBuilder(module);

    auto gvIt = module.global_begin();
    while (gvIt != module.global_end()) {
        GlobalVariable& gv = *(gvIt++);

        auto type = gv.getType()->getElementType();
        AllocaInst* alloc = builder.CreateAlloca(type, nullptr, gv.getName());

        Value* init = gv.hasInitializer() ? gv.getInitializer() : UndefValue::get(type);
        StoreInst* store = builder.CreateStore(init, alloc);

        // Add some metadata stuff
        if (gv.hasMetadata()) {
            llvm::SmallVector<std::pair<unsigned, llvm::MDNode*>, 2> metadata;
            gv.getAllMetadata(metadata);
        }

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