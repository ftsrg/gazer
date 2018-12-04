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

    auto fType = llvm::FunctionType::get(llvm::Type::getVoidTy(module.getContext()), {
        llvm::Type::getMetadataTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    }, false);

    llvm::Constant* mark = module.getOrInsertFunction(
        "gazer.inlined_global.write", fType
    );

    Function* dbgDecl = Intrinsic::getDeclaration(&module, Intrinsic::dbg_declare);

    IRBuilder<> builder(module.getContext());
    builder.SetInsertPoint(&main->getEntryBlock(), main->getEntryBlock().begin());

    DIBuilder diBuilder(module);

    auto gvIt = module.global_begin();
    while (gvIt != module.global_end()) {
        GlobalVariable& gv = *(gvIt++);
        if (gv.isConstant()) {
            continue;
        }

        auto type = gv.getType()->getElementType();
        AllocaInst* alloc = builder.CreateAlloca(type, nullptr, gv.getName());

        Value* init = gv.hasInitializer() ? gv.getInitializer() : UndefValue::get(type);
        StoreInst* store = builder.CreateStore(init, alloc);

        // Add some metadata stuff
        // FIXME: There should be a more intelligent way for finding
        //  the DIGlobalVariable
        llvm::SmallVector<std::pair<unsigned, MDNode*>, 2> md;
        gv.getAllMetadata(md);

        llvm::DIGlobalVariableExpression* diGlobalExpr = nullptr;
        std::for_each(md.begin(), md.end(), [&diGlobalExpr](auto pair) {
            if (auto ge = dyn_cast<DIGlobalVariableExpression>(pair.second)) {
                diGlobalExpr = ge;
            }
        });

        if (diGlobalExpr) {
            auto diGlobalVariable = diGlobalExpr->getVariable();
            for (llvm::Value* user : gv.users()) {
                if (auto inst = llvm::dyn_cast<StoreInst>(user)) {
                    llvm::Value* value = inst->getOperand(0);
                    CallInst* call = CallInst::Create(
                        fType, mark, {
                            MetadataAsValue::get(module.getContext(), ValueAsMetadata::get(value)),
                            MetadataAsValue::get(module.getContext(), diGlobalVariable)
                        }
                    );

                    call->setDebugLoc(inst->getDebugLoc());
                    call->insertAfter(inst);
                }
            }
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