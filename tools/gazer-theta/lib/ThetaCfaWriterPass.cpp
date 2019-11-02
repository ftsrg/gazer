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
#include "ThetaCfaGenerator.h"

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/Pass.h>

using namespace gazer;

namespace
{

struct ThetaCfaWriterPass : public llvm::ModulePass
{
    static char ID;
    llvm::raw_ostream& mOS;

    ThetaCfaWriterPass(llvm::raw_ostream& os)
        : ModulePass(ID), mOS(os)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<ModuleToAutomataPass>();
        au.setPreservesAll();
    }

    bool runOnModule(llvm::Module& module) override
    {
        auto& system = getAnalysis<ModuleToAutomataPass>().getSystem();

        theta::ThetaNameMapping mapping;
        theta::ThetaCfaGenerator generator{system};

        generator.write(mOS, mapping);

        return false;
    }


};

} // end anonymous namespace

char ThetaCfaWriterPass::ID;

llvm::Pass* gazer::theta::createThetaCfaWriterPass(llvm::raw_ostream& os)
{
    return new ThetaCfaWriterPass(os);
}