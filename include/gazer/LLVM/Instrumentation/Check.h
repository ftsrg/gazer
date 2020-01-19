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
#ifndef GAZER_LLVM_INSTRUMENTATION_CHECK_H
#define GAZER_LLVM_INSTRUMENTATION_CHECK_H

#include <llvm/Pass.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>

namespace gazer
{

class CheckRegistry;

/// A check is a special kind of an LLVM pass, which marks instrunctions 
/// with pre- or postconditions that must be always true.
class Check : public llvm::ModulePass
{
    friend class CheckRegistry;
public:
    explicit Check(char& id)
        : ModulePass(id)
    {}

    Check(const Check&) = delete;
    Check& operator=(const Check&) = delete;

    bool runOnModule(llvm::Module& module) final;

    /// Returns this check's name. This name will be used to identify
    /// the check through the command-line, thus this should be short,
    /// descriptive and must not contain whitespaces,
    /// e.g.: "assert-fail", "div-by-zero", "int-overflow"
    virtual llvm::StringRef getCheckName() const = 0;

    /// Returns a user-friendly error description on why this particular
    /// check failed. Such descriptions should be short and simple, e.g.
    /// "Assertion failure", "Division by zero", or "Signed integer overflow".
    virtual llvm::StringRef getErrorDescription() const = 0;

    /// Marks the given function's instructions with required
    /// pre- and postconditions.
    virtual bool mark(llvm::Function& function) = 0;

protected:

    /// Creates an error block with a gazer.error_code(i16 code) call and a terminating unreachable instruction.
    llvm::BasicBlock* createErrorBlock(
        llvm::Function& function, const llvm::Twine& name = "", llvm::Instruction* location = nullptr
    );

    CheckRegistry& getRegistry() const;

private:
    void setCheckRegistry(CheckRegistry& registry);

private:
    CheckRegistry* mRegistry = nullptr;
};

class CheckViolation
{
public:
    CheckViolation(Check* check, llvm::DebugLoc location)
        : mCheck(check), mLocation(location)
    {
        assert(check != nullptr);
    }

    CheckViolation(const CheckViolation&) = default;
    CheckViolation& operator=(const CheckViolation&) = default;

    Check* getCheck() const { return mCheck; }
    llvm::DebugLoc getDebugLoc() const { return mLocation; }

private:
    Check* mCheck;
    llvm::DebugLoc mLocation;
};

class CheckRegistry
{
public:
    static constexpr char ErrorFunctionName[] = "gazer.error_code";
public:
    explicit CheckRegistry(llvm::LLVMContext& context)
        : mLlvmContext(context)
    {}

    CheckRegistry(const CheckRegistry&) = delete;
    CheckRegistry& operator=(const CheckRegistry&) = delete;

    static llvm::FunctionCallee GetErrorFunction(llvm::Module& module);
    static llvm::FunctionCallee GetErrorFunction(llvm::Module* module) {
        return GetErrorFunction(*module);
    }

    static llvm::FunctionType* GetErrorFunctionType(llvm::LLVMContext& context);
    static llvm::IntegerType* GetErrorCodeType(llvm::LLVMContext& context);

public:
    /// Inserts a check into the check registry.
    /// This class will take ownership of the check object.
    void add(Check* check);

    /// Registers all enabled checks into the pass manager.
    /// As the pass manager takes ownership of all registered
    /// passes, this class will release all enabled checks
    /// after calling this method.
    void registerPasses(llvm::legacy::PassManager& pm);

    /// Creates a new check violation with a unique error code
    /// for a given check and location.
    /// \return An LLVM value representing the error code.
    llvm::Value* createCheckViolation(Check* check, llvm::DebugLoc loc);

    std::string messageForCode(unsigned ec) const;

    ~CheckRegistry();
private:
    llvm::LLVMContext& mLlvmContext;
    llvm::DenseMap<unsigned, CheckViolation> mCheckMap;
    std::vector<Check*> mChecks;
    bool mRegisterPassesCalled = false;

    // 0 stands for success, 1 for unknown failures.
    unsigned mErrorCodeCnt = 2;
};

} // end namespace gazer

#endif