#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>

using namespace gazer;
using namespace llvm;

bool Check::runOnModule(llvm::Module& module)
{
    assert(mRegistry != nullptr
        && "The check registry was not set before check execution was initiated."
        "Did you forget to add the check to the CheckRegistry?");

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
    Function& function, const Twine& name, llvm::Instruction* location
) {
    IRBuilder<> builder(function.getContext());
    BasicBlock* errorBB = BasicBlock::Create(
        function.getContext(), name, &function
    );

    builder.SetInsertPoint(errorBB);

    llvm::DebugLoc dbgLoc;
    if (location != nullptr && location->getDebugLoc()) {
        dbgLoc = location->getDebugLoc();
    }

    auto errorCode = mRegistry->createCheckViolation(this, dbgLoc);

    auto call = builder.CreateCall(
        CheckRegistry::GetErrorFunction(function.getParent()),
        { errorCode }
    );
    builder.CreateUnreachable();

    return errorBB;
}

CheckRegistry& Check::getRegistry() const { return *mRegistry; }

void Check::setCheckRegistry(CheckRegistry& registry)
{
    mRegistry = &registry;
}

llvm::Value* CheckRegistry::createCheckViolation(Check* check, llvm::DebugLoc loc)
{
    unsigned ec = mErrorCodeCnt++;
    mCheckMap.try_emplace(ec, check, loc);

    return llvm::ConstantInt::get(
        llvm::IntegerType::get(mLlvmContext, 16),
        llvm::APInt{16, ec}
    );
}

llvm::FunctionType* CheckRegistry::GetErrorFunctionType(llvm::LLVMContext& context)
{
    return llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        llvm::Type::getInt16Ty(context),
        /*isVarArg=*/false
    );
}

llvm::FunctionCallee CheckRegistry::GetErrorFunction(llvm::Module& module)
{
    return module.getOrInsertFunction(
        ErrorFunctionName,
        GetErrorFunctionType(module.getContext())
    );
}

void CheckRegistry::add(Check* check)
{
    mCheckNames[check->getCheckName()] = check;

    check->setCheckRegistry(*this);
    mChecks.push_back(check);
}

void CheckRegistry::registerPasses(llvm::legacy::PassManager& pm)
{
    for (Check* check : mChecks) {
        pm.add(check);
    }
}

std::string CheckRegistry::messageForCode(unsigned ec) const
{
    auto result = mCheckMap.find(ec);
    assert(result != mCheckMap.end() && "Error code should be present in the check map!");

    auto violation = result->second;
    auto location = violation.getDebugLoc();
    
    std::string buffer;
    llvm::raw_string_ostream rso(buffer);

    rso << violation.getCheck()->getErrorDescription();

    if (location) {
        auto fname = location->getFilename();
        rso
            << " in " << (fname != "" ? fname : "<unknown file>")
            << " at line " << location->getLine()
            << " column " << location->getColumn();
    }
    rso << ".";

    return rso.str();
}
