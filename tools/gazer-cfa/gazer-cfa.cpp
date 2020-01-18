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
/// \file A simple tool which dumps a gazer CFA translated from
/// an input LLVM IR file.
//===----------------------------------------------------------------------===//

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/ClangFrontend.h"

#include <llvm/IR/Module.h>

#ifndef NDEBUG
#include <llvm/Support/Debug.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/PrettyStackTrace.h>
#endif

using namespace gazer;
using namespace llvm;

namespace
{
    cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore, cl::desc("<input files>"));
    cl::opt<bool> ViewCfa("view", cl::desc("View the CFA in the system's GraphViz viewier."));
    cl::opt<bool> CyclicCfa("cyclic", cl::desc("Represent LoopRep as cycles instead of recursive calls."));
    cl::opt<bool> RunPipeline("run-pipeline", cl::desc("Run the early stages of the verification pipeline, such as instrumentation."));
}

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    FrontendConfigWrapper config;
    auto frontend = config.buildFrontend(InputFilenames);

    if (CyclicCfa) {
        frontend->getSettings().loops = LoopRepresentation::Cycle;
    }

    if (RunPipeline) {
        frontend->registerVerificationPipeline();
    } else {
        frontend->registerPass(new gazer::ModuleToAutomataPass(config.context, frontend->getSettings()));
        frontend->registerPass(gazer::createCfaPrinterPass());
    }
    
    if (ViewCfa) {
        frontend->registerPass(gazer::createCfaViewerPass());
    }

    frontend->run();

    llvm::llvm_shutdown();
}
