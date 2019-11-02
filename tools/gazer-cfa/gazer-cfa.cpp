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

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

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
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    cl::opt<bool> ViewCfa("view", cl::desc("View the CFA in the system's GraphViz viewier."));
    cl::opt<bool> CyclicCfa("cyclic", cl::desc("Represent LoopRep as cycles instead of recursive calls."));
    cl::opt<bool> RunPipeline("run-pipeline", cl::desc("Run the early stages of the verification pipeline, such as instrumentation."));
}

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    
    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    GazerContext context;
    llvm::LLVMContext llvmContext;

    auto settings = LLVMFrontendSettings::initFromCommandLine();
    if (CyclicCfa) {
        settings.loops = LoopRepresentation::Cycle;
    }

    auto frontend = LLVMFrontend::FromInputFile(InputFilename, context, llvmContext, settings);
    if (frontend == nullptr) {
        return 1;
    }

    frontend->registerPass(new gazer::ModuleToAutomataPass(context, settings));
    frontend->registerPass(gazer::createCfaPrinterPass());
    if (ViewCfa) {
        frontend->registerPass(gazer::createCfaViewerPass());
    }

    frontend->run();

    llvm::llvm_shutdown();
}
