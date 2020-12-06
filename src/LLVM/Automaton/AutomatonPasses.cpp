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
#include "gazer/LLVM/Automaton/SpecialFunctions.h"
#include "gazer/LLVM/Memory/MemoryModel.h"

#include <gazer/LLVM/LLVMFrontendSettingsProviderPass.h>

using namespace gazer;
using namespace gazer::llvm2cfa;

std::unique_ptr<AutomataSystem> gazer::translateModuleToAutomata(
    llvm::Module& module,
    const LLVMFrontendSettings& settings,
    LoopInfoFuncTy loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    CfaToLLVMTrace& blockEntries,
    const SpecialFunctions* specialFunctions)
{
    if (specialFunctions == nullptr) {
        specialFunctions = &SpecialFunctions::empty();
    }

    LLVMTypeTranslator types(memoryModel.getMemoryTypeTranslator(), settings);
    ModuleToCfa transformer(module, std::move(loopInfos), context, memoryModel, types, *specialFunctions, settings);
    return transformer.generate(variables, blockEntries);
}

// LLVM pass implementation
//-----------------------------------------------------------------------------

char ModuleToAutomataPass::ID;

void ModuleToAutomataPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<LLVMFrontendSettingsProviderPass>();
    au.addRequired<llvm::DominatorTreeWrapperPass>();
    au.addRequired<MemoryModelWrapperPass>();
    au.setPreservesAll();
}

bool ModuleToAutomataPass::runOnModule(llvm::Module& module)
{
    auto& settings = getAnalysis<LLVMFrontendSettingsProviderPass>()
        .getSettings();
    // We need to save loop information here as a on-the-fly LoopInfo pass would delete
    // the acquired loop information when the lambda function exits.
    llvm::DenseMap<const llvm::Function*, std::unique_ptr<llvm::LoopInfo>> loopInfos;
    for (const llvm::Function& function : module) {
        if (!function.isDeclaration()) {
            // The const_cast is needed here as getAnalysis expects a non-const function.
            // However, it should be safe as DominatorTreeWrapper does not modify the function.
            auto& dt =
                getAnalysis<llvm::DominatorTreeWrapperPass>(*const_cast<llvm::Function*>(&function)).getDomTree();
            loopInfos.try_emplace(&function, std::make_unique<llvm::LoopInfo>(dt));
        }
    }

    auto loops = [&loopInfos](const llvm::Function* function) -> llvm::LoopInfo* {
        auto& result = loopInfos[function];
        assert(result != nullptr);
        return result.get();
    };

    MemoryModel& memoryModel = getAnalysis<MemoryModelWrapperPass>().getMemoryModel();
    auto specialFunctions = SpecialFunctions::get();

    mSystem = translateModuleToAutomata(
        module, settings, loops, mContext, memoryModel, mVariables, mTraceInfo, specialFunctions.get());

    if (settings.loops == LoopRepresentation::Cycle) {
        // Transform the main automaton into a cyclic CFA if requested.
        // Note: This yields an invalid CFA, which will not be recognizable by
        // most built-in analysis algorithms. Use it only if you are going to
        // translate it to the format of another verifier immediately.

        // TODO: We should translate automata other than the main in this case.
        TransformRecursiveToCyclic(mSystem->getMainAutomaton());
    }

    return false;
}

namespace {

class PrintCfaPass : public llvm::ModulePass
{
    public:
    static char ID;

    PrintCfaPass() : ModulePass(ID) {}

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

    ViewCfaPass() : ModulePass(ID) {}

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

llvm::Pass* gazer::createCfaPrinterPass()
{
    return new PrintCfaPass();
}
llvm::Pass* gazer::createCfaViewerPass()
{
    return new ViewCfaPass();
}

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