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
#ifndef GAZER_MEMORY_MEMORYMODEL_H
#define GAZER_MEMORY_MEMORYMODEL_H

#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/Memory/MemoryInstructionHandler.h"

namespace llvm
{
    class Loop;
} // end namespace llvm

namespace gazer
{

class Cfa;

/// Memory models are responsible for representing memory-related types and instructions.
class MemoryModel
{
public:
    MemoryModel() = default;

    MemoryModel(const MemoryModel&) = delete;
    MemoryModel& operator=(const MemoryModel&) = delete;

    /// Returns the memory instruction translator of this memory model.
    virtual MemoryInstructionHandler& getMemoryInstructionHandler(llvm::Function& function) = 0;

    /// Returns the type translator of this memory model.
    virtual MemoryTypeTranslator& getMemoryTypeTranslator() = 0;

    virtual ~MemoryModel() = default;
};

//==------------------------------------------------------------------------==//
/// HavocMemoryModel - A havoc memory model which does not create any memory
/// objects. Load operations return an unknown value and store instructions
/// have no effect. No MemoryObjectPhi's are inserted.
std::unique_ptr<MemoryModel> CreateHavocMemoryModel(GazerContext& context);

//==-----------------------------------------------------------------------==//
// FlatMemoryModel - a memory model which represents all memory as a single
// array, where loads and stores are reads and writes in said array.
std::unique_ptr<MemoryModel> CreateFlatMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    llvm::Module& llvmModule,
    std::function<llvm::DominatorTree&(llvm::Function&)> dominators
);

class MemoryModelWrapperPass : public llvm::ModulePass
{
public:
    static char ID;

    MemoryModelWrapperPass(GazerContext& context, const LLVMFrontendSettings& settings)
        : ModulePass(ID), mContext(context), mSettings(settings)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    bool runOnModule(llvm::Module& llvmModule) override;

    MemoryModel& getMemoryModel() const { return *mMemoryModel; }

    llvm::StringRef getPassName() const override {
        return "Memory model wrapper pass";
    }

private:
    GazerContext& mContext;
    const LLVMFrontendSettings& mSettings;
    std::unique_ptr<MemoryModel> mMemoryModel;
};


} // namespace gazer
#endif //GAZER_MEMORY_MEMORYMODEL_H
