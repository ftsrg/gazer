//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/ADT/StringExtras.h>

#include "gazer/Trace/VerificationResult.h"

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

    builder.CreateCall(
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
        GetErrorCodeType(context),
        /*isVarArg=*/false
    );
}

llvm::IntegerType* CheckRegistry::GetErrorCodeType(llvm::LLVMContext& context)
{
    return llvm::Type::getInt16Ty(context);
}

llvm::FunctionCallee CheckRegistry::GetErrorFunction(llvm::Module& module)
{
    llvm::AttrBuilder ab;
    ab.addAttribute(llvm::Attribute::NoReturn);

    return module.getOrInsertFunction(
        ErrorFunctionName,
        GetErrorFunctionType(module.getContext()),
        llvm::AttributeList::get(module.getContext(), AttributeList::FunctionIndex, ab)
    );
}

void CheckRegistry::add(Check* check)
{
    check->setCheckRegistry(*this);
    mChecks.push_back(check);
}

void CheckRegistry::registerPasses(llvm::legacy::PassManager& pm)
{
    for (Check* check : mChecks) {
        pm.add(check);
    }
    mRegisterPassesCalled = true;
}

std::string CheckRegistry::messageForCode(unsigned ec) const
{
    assert(ec != VerificationResult::SuccessErrorCode && "Error code must be non-zero for failures!");

    if (ec == VerificationResult::GeneralFailureCode) {
        return "Unknown failure: a property violation was found, but I could not create an error trace.\n";
    }

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

CheckRegistry::~CheckRegistry()
{
    // If registerPasses() was not called, this object still owns all added checks.
    if (!mRegisterPassesCalled) {
        for (Check* check : mChecks) {
            delete check;
        }
    }
}
