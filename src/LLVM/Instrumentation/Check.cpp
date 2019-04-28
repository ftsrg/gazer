#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace gazer;
using namespace llvm;

bool Check::runOnModule(llvm::Module& module)
{
    bool changed = false;

    for (llvm::Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        // Find all the markings
        changed |= this->mark(function);
    }

    return changed;
}

llvm::BasicBlock* Check::createErrorBlock(
    Function& function, llvm::Value* errorCode,
    const Twine& name, llvm::Instruction* location
) {
    IRBuilder<> builder(function.getContext());
    BasicBlock* errorBB = BasicBlock::Create(
        function.getContext(), name, &function
    );
    builder.SetInsertPoint(errorBB);
    auto call = builder.CreateCall(
        CheckRegistry::GetErrorFunction(function.getParent()),
        { errorCode }
    );
    builder.CreateUnreachable();

    if (location != nullptr && location->getDebugLoc()) {
        call->setMetadata(
            llvm::Metadata::DILocationKind, location->getDebugLoc().getAsMDNode()
        );
    }

    return errorBB;
}

CheckRegistry CheckRegistry::Instance;

llvm::FunctionType* CheckRegistry::GetErrorFunctionType(llvm::LLVMContext& context)
{
    return llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        { llvm::Type::getInt16Ty(context) }
    );
}

llvm::FunctionCallee CheckRegistry::GetErrorFunction(llvm::Module& module)
{
    return module.getOrInsertFunction(
        "gazer.error_code",
        GetErrorFunctionType(module.getContext())
    );
}

unsigned CheckRegistry::getErrorCode(char& id)
{
    const void* idPtr = static_cast<const void*>(&id);
    auto result = mErrorCodes.find(idPtr);
    
    assert(result != mErrorCodes.end()
        && "Attempting to get code for an unregistered check type");

    return result->second;
}

void CheckRegistry::add(Check* check)
{
    unsigned ec = mErrorCodeCnt++;
    const void* pID = check->getPassID();
    mErrorCodes[pID] = ec;
    mCheckMap[ec] = check;
    mCheckNames[check->getCheckName()] = check;
    mChecks.push_back(check);
}

void CheckRegistry::registerPasses(llvm::legacy::PassManager& pm)
{
    for (Check* check : mChecks) {
        pm.add(check);
    }
}

std::string CheckRegistry::messageForCode(unsigned ec)
{
    auto result = mCheckMap.find(ec);
    assert(result != mCheckMap.end() && "Error code should be present in the check map");

    return result->second->getErrorDescription();
}
