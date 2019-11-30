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
///
/// \file This file contains LLVM passes related to gazer's CFA formalism.
///
//===----------------------------------------------------------------------===//

#include "FunctionToCfa.h"

#include "gazer/Automaton/CfaTransforms.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Memory/MemoryModel.h"

using namespace gazer;
using namespace gazer::llvm2cfa;

std::unique_ptr<AutomataSystem> gazer::translateModuleToAutomata(
    llvm::Module& module,
    LLVMFrontendSettings settings,
    llvm::DenseMap<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    CfaToLLVMTrace& blockEntries
) {
    ModuleToCfa transformer(module, loopInfos, context, memoryModel, settings);
    return transformer.generate(variables, blockEntries);
}

// LLVM pass implementation
//-----------------------------------------------------------------------------

char ModuleToAutomataPass::ID;

void ModuleToAutomataPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<llvm::DominatorTreeWrapperPass>();
    au.setPreservesAll();
}

bool ModuleToAutomataPass::runOnModule(llvm::Module& module)
{
    GenerationContext::LoopInfoMapTy loopInfoMap;
    std::vector<std::unique_ptr<llvm::DominatorTree>> dominators;
    std::vector<std::unique_ptr<llvm::LoopInfo>> loops;

    for (llvm::Function& function : module) {
        if (!function.isDeclaration()) {
            auto& dt = dominators.emplace_back(std::make_unique<llvm::DominatorTree>(function));
            auto& loop = loops.emplace_back(std::make_unique<llvm::LoopInfo>(*dt));

            loopInfoMap[&function] = loop.get();
        }
    }

    auto memoryModel = CreateBasicMemoryModel(mContext, mSettings, module.getDataLayout());
    memoryModel->initialize(module, [this](llvm::Function& function) -> llvm::DominatorTree& {
        return getAnalysis<llvm::DominatorTreeWrapperPass>(function).getDomTree();
    });

    if (mSettings.debugDumpMemorySSA) {
        for (auto& function : module.functions()) {
            if (function.isDeclaration()) {
                continue;
            }

            auto memSSA = memoryModel->getFunctionMemorySSA(function);
            memSSA->print(llvm::errs());
        }
    }

    mSystem = translateModuleToAutomata(
        module, mSettings, loopInfoMap, mContext, *memoryModel, mVariables, mTraceInfo
    );

    if (mSettings.loops == LoopRepresentation::Cycle) {
        // Transform the main automaton into a cyclic CFA if requested.
        // Note: This yields an invalid CFA, which will not be recognizable by
        // most built-in analysis algorithms. Use it only if you are going to
        // translate it to the format of another verifier immediately.

        // TODO: We should translate automata other than the main in this case.
        TransformRecursiveToCyclic(mSystem->getMainAutomaton());
    }

    return false;
}

namespace
{

class PrintCfaPass : public llvm::ModulePass
{
public:
    static char ID;

    PrintCfaPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<ModuleToAutomataPass>();
        au.setPreservesAll();
    }

    bool runOnModule(llvm::Module& module) override
    {
        auto& moduleToCfa = getAnalysis<ModuleToAutomataPass>();
        AutomataSystem& system = moduleToCfa.getSystem();

        system.print(llvm::outs());

        return false;
    }
};

class ViewCfaPass : public llvm::ModulePass
{
public:
    static char ID;

    ViewCfaPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<ModuleToAutomataPass>();
        au.setPreservesAll();
    }

    bool runOnModule(llvm::Module& module) override
    {
        auto& moduleToCfa = getAnalysis<ModuleToAutomataPass>();
        AutomataSystem& system = moduleToCfa.getSystem();

        for (Cfa& cfa : system) {
            cfa.view();
        }

        return false;
    }
};

} // end anonymous namespace

char PrintCfaPass::ID;
char ViewCfaPass::ID;

llvm::Pass* gazer::createCfaPrinterPass() { return new PrintCfaPass(); }
llvm::Pass* gazer::createCfaViewerPass()  { return new ViewCfaPass();  }

// Traceability support
//-----------------------------------------------------------------------------

ExprPtr CfaToLLVMTrace::getExpressionForValue(const Cfa* parent, const llvm::Value* value)
{
    auto it = mValueMaps.find(parent);
    if (it == mValueMaps.end()) {
        return nullptr;
    }

    ExprPtr expr = it->second.values.lookup(value);

    return expr;
}

Variable* CfaToLLVMTrace::getVariableForValue(const Cfa* parent, const llvm::Value* value)
{
    auto expr = getExpressionForValue(parent, value);
    if (auto varRef = llvm::dyn_cast_or_null<VarRefExpr>(expr)) {
        return &varRef->getVariable();
    }

    return nullptr;
}