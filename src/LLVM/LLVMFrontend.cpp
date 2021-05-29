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
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Transform/UndefToNondet.h"
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/Trace/TraceWriter.h"
#include "gazer/LLVM/Trace/TestHarnessGenerator.h"
#include "gazer/Trace/WitnessWriter.h"
#include "gazer/Support/Warnings.h"

#include <llvm/Analysis/ScopedNoAliasAA.h>
#include <llvm/Analysis/TypeBasedAliasAnalysis.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/InitializePasses.h>
#include <llvm/Analysis/CallGraph.h>

#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/Analysis/CFGPrinter.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/BitcodeWriter.h>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    extern cl::OptionCategory LLVMFrontendCategory;
} // end namespace gazer

namespace
{
    cl::opt<bool> ShowFinalCFG(
        "show-final-cfg", cl::desc("Display the final CFG"), cl::cat(LLVMFrontendCategory));
    cl::opt<std::string> PrintFinalModule(
        "print-final-module", cl::desc("Output final module into file"), cl::cat(LLVMFrontendCategory)
    );
    cl::opt<bool> StructurizeCFG(
        "structurize", cl::desc("Try to remove irreducible controlf flow"), cl::cat(LLVMFrontendCategory)
    );
    cl::opt<bool> SkipPipeline(
        "skip-pipeline", cl::desc("Do not execute the verification pipeline; translate and verify the input LLVM module directly")
    );

    class RunVerificationBackendPass : public llvm::ModulePass
    {
    public:
        static char ID;

        RunVerificationBackendPass(
            const CheckRegistry& checks,
            VerificationAlgorithm& algorithm,
            const LLVMFrontendSettings& settings
        ) : ModulePass(ID), mChecks(checks), mAlgorithm(algorithm), mSettings(settings)
        {}

        void getAnalysisUsage(llvm::AnalysisUsage& au) const override
        {
            au.addRequired<ModuleToAutomataPass>();
            au.setPreservesCFG();
        }

        bool runOnModule(llvm::Module& module) override;

        llvm::StringRef getPassName() const override {
            return "Verification backend pass";
        }

    private:
        const CheckRegistry& mChecks;
        VerificationAlgorithm& mAlgorithm;
        const LLVMFrontendSettings& mSettings;
        std::unique_ptr<VerificationResult> mResult;
    };

} // end anonymous namespace

char RunVerificationBackendPass::ID;

LLVMFrontend::LLVMFrontend(
    std::unique_ptr<llvm::Module> llvmModule,
    GazerContext& context,
    LLVMFrontendSettings& settings)
    : mContext(context),
    mModule(std::move(llvmModule)),
    mChecks(mModule->getContext()),
    mSettings(settings)
{
    llvm::initializeAnalysis(*llvm::PassRegistry::getPassRegistry());

    // Force settings to be consistent
    if (mSettings.ints == IntRepresentation::Integers) {
        emit_warning("-math-int mode forces havoc memory model, analysis may be unsound\n");
        mSettings.memoryModel = MemoryModelSetting::Havoc;
    }

    if (!PrintFinalModule.empty()) {
        std::error_code ec;
        mModuleOutput = std::make_unique<llvm::ToolOutputFile>(PrintFinalModule, ec, llvm::sys::fs::F_None);

        if (ec) {
            emit_error("could not open '%s': %s", PrintFinalModule.c_str(), ec.message().c_str());
            mModuleOutput = nullptr;
        }
    }
}

void LLVMFrontend::registerVerificationPipeline()
{
    if (SkipPipeline) {
        mPassManager.add(new llvm::DominatorTreeWrapperPass());
        this->registerVerificationStep();
        return;
    }

    // Do basic preprocessing: get rid of alloca's and turn undef's
    //  into nondet function calls.
    mPassManager.add(llvm::createPromoteMemoryToRegisterPass());
    mPassManager.add(gazer::createPromoteUndefsPass());

    // Perform check instrumentation.
    registerEnabledChecks();

    // Execute early optimization passes.
    registerEarlyOptimizations();

    // Inline functions and global variables if requested.
    mPassManager.add(gazer::createMarkFunctionEntriesPass());
    registerInlining();

    // Unify function exit nodes
    mPassManager.add(llvm::createUnifyFunctionExitNodesPass());

    // Run assertion lifting.
    if (mSettings.liftAsserts) {
        mPassManager.add(new llvm::CallGraphWrapperPass());
        mPassManager.add(gazer::createLiftErrorCallsPass(*mSettings.getEntryFunction(*mModule)));

        // Assertion lifting creates a lot of dead code. Run a lightweight DCE pass 
        // and a subsequent CFG simplification to clean up.
        mPassManager.add(llvm::createDeadCodeEliminationPass());
        mPassManager.add(llvm::createCFGSimplificationPass());

        // FIXME: Run program slicing here if requested.
    }

    // Execute late optimization passes.
    registerLateOptimizations();

    // Do an instruction namer pass.
    mPassManager.add(llvm::createInstructionNamerPass());

    // Unify exit nodes again
    mPassManager.add(llvm::createUnifyFunctionExitNodesPass());
    mPassManager.add(llvm::createInstructionCombiningPass(false));
    
    // Display the final LLVM CFG now.
    if (ShowFinalCFG) {
        mPassManager.add(llvm::createCFGPrinterLegacyPassPass());
    }

    if (mModuleOutput != nullptr) {
        mPassManager.add(llvm::createPrintModulePass(mModuleOutput->os()));
        mModuleOutput->keep();
    }

    this->registerVerificationStep();
}

void LLVMFrontend::registerVerificationStep()
{
    // Perform module-to-automata translation.
    mPassManager.add(new gazer::MemoryModelWrapperPass(mContext, mSettings));
    mPassManager.add(new gazer::ModuleToAutomataPass(mContext, mSettings));

    // Execute the verifier backend if there is one.
    if (mBackendAlgorithm != nullptr) {
        mPassManager.add(new RunVerificationBackendPass(mChecks, *mBackendAlgorithm, mSettings));
    }
}

bool RunVerificationBackendPass::runOnModule(llvm::Module& module)
{
    auto& moduleToCfa = getAnalysis<ModuleToAutomataPass>();

    AutomataSystem& system = moduleToCfa.getSystem();
    CfaToLLVMTrace cfaToLlvmTrace = moduleToCfa.getTraceInfo();
    LLVMTraceBuilder traceBuilder{system.getContext(), cfaToLlvmTrace};

    mResult = mAlgorithm.check(system, traceBuilder);
    switch (mResult->getStatus()) {
        case VerificationResult::Fail: {
            auto fail = llvm::cast<FailResult>(mResult.get());
            unsigned ec = fail->getErrorID();
            std::string msg = mChecks.messageForCode(ec);

            llvm::outs() << "Verification FAILED.\n";
            llvm::outs() << "  " << msg << "\n";

            if (mSettings.trace) {
                auto writer = trace::CreateTextWriter(llvm::outs(), true);
                llvm::outs() << "Error trace:\n";
                llvm::outs() << "------------\n";
                if (fail->hasTrace()) {
                    writer->write(fail->getTrace());
                } else {
                    llvm::outs() << "Error trace is unavailable.\n";
                }
            }
            
            if (!mSettings.witness.empty() && fail->hasTrace() && !mSettings.hash.empty()) {
                if (fail->hasTrace()) {
                    std::error_code EC{};
                    llvm::raw_fd_ostream fouts{ StringRef{mSettings.witness}, EC };
                    
                    ViolationWitnessWriter witnessWriter{ fouts, mSettings.hash };
                    
                    witnessWriter.initializeWitness();
                    witnessWriter.write(fail->getTrace());
                    witnessWriter.closeWitness();
                } else if (mSettings.hash.empty()) {
                    llvm::outs() << "Hash of the source file must be given to produce a witness (--hash)";
                } else if (!mSettings.witness.empty()) {
                    llvm::outs() << "Error witness is unavailable.\n";
                }
            }

            if (!mSettings.testHarnessFile.empty() && fail->hasTrace()) {
                llvm::outs() << "Generating test harness.\n";
                auto test = GenerateTestHarnessModuleFromTrace(
                    fail->getTrace(), 
                    module.getContext(),
                    module
                );

                llvm::StringRef filename(mSettings.testHarnessFile);
                std::error_code osError;
                llvm::raw_fd_ostream testOS(filename, osError, llvm::sys::fs::OpenFlags::OF_None);

                if (filename.endswith("ll")) {
                    testOS << *test;
                } else {
                    llvm::WriteBitcodeToFile(*test, testOS);
                }
            }
            break;
        }
        case VerificationResult::Success:
            llvm::outs() << "Verification SUCCESSFUL.\n";
            if (!mSettings.witness.empty() && !mSettings.hash.empty()) {
                // puts the witness file in the working directory of gazer
                std::error_code EC{};
                llvm::raw_fd_ostream fouts{ StringRef{mSettings.witness}, EC };
                
                CorrectnessWitnessWriter witnessWriter{ fouts, mSettings.hash };
                witnessWriter.outputWitness();
            } else if(!mSettings.witness.empty() && mSettings.hash.empty()) {
                llvm::outs() << "Hash of the source file must be given to produce a witness (--hash)";
            }
            break;
        case VerificationResult::Timeout:
            llvm::outs() << "Verification TIMEOUT.\n";
            break;
        case VerificationResult::BoundReached:
            llvm::outs() << "Verification BOUND REACHED.\n";
            break;
        case VerificationResult::InternalError:
            llvm::outs() << "Verification INTERNAL ERROR.\n";
            llvm::outs() << "  " << mResult->getMessage() << "\n";
            break;
        case VerificationResult::Unknown:
            llvm::outs() << "Verification UNKNOWN.\n";
            break;
    }

    return false;
}

void LLVMFrontend::registerEnabledChecks()
{
    mChecks.registerPasses(mPassManager);
}

void LLVMFrontend::registerInlining()
{
    if (mSettings.inlineLevel != InlineLevel::Off) {
        mPassManager.add(llvm::createInternalizePass([this](auto& gv) {
            if (auto fun = llvm::dyn_cast<llvm::Function>(&gv)) {
                return mSettings.getEntryFunction(*gv.getParent()) == fun;
            }
            return false;
        }));
        mPassManager.add(gazer::createSimpleInlinerPass(*mSettings.getEntryFunction(*mModule), mSettings.inlineLevel));

        // Remove dead functions
        mPassManager.add(llvm::createGlobalDCEPass());

        // Inline eligible global variables
        if (mSettings.inlineGlobals) {
            mPassManager.add(gazer::createInlineGlobalVariablesPass());
        }

        // Remove dead globals
        mPassManager.add(llvm::createGlobalDCEPass());

        // Transform the generated alloca instructions into registers
        mPassManager.add(llvm::createPromoteMemoryToRegisterPass());
    }
}

void LLVMFrontend::registerPass(llvm::Pass* pass)
{
    mPassManager.add(pass);
}

void LLVMFrontend::run()
{
    mPassManager.run(*mModule);
}

void LLVMFrontend::registerEarlyOptimizations()
{
    if (!mSettings.optimize) {
        return;
    }

    // Start with some metadata-based typed AA
    mPassManager.add(llvm::createTypeBasedAAWrapperPass());
    mPassManager.add(llvm::createScopedNoAliasAAWrapperPass());

    // Split call sites under conditionals
    mPassManager.add(llvm::createCallSiteSplittingPass());

    // Do some inter-procedural reductions
    mPassManager.add(llvm::createIPSCCPPass());
    mPassManager.add(llvm::createGlobalOptimizerPass());
    mPassManager.add(llvm::createDeadArgEliminationPass());

    // Clean up
    mPassManager.add(llvm::createInstructionCombiningPass());
    mPassManager.add(llvm::createCFGSimplificationPass());

    // SROA may introduce new undef values, so we run another promote undef pass after it
    mPassManager.add(llvm::createSROAPass());
    mPassManager.add(gazer::createPromoteUndefsPass());
    //mPassManager.add(llvm::createEarlyCSEPass());

    mPassManager.add(llvm::createCFGSimplificationPass());
    mPassManager.add(llvm::createAggressiveInstCombinerPass());
    mPassManager.add(llvm::createInstructionCombiningPass());

    // Try to remove irreducible control flow
    if (StructurizeCFG) {
        mPassManager.add(llvm::createStructurizeCFGPass());
    }
    
    // Optimize loops
    //mPassManager.add(llvm::createLoopInstSimplifyPass());
    //mPassManager.add(llvm::createLoopSimplifyCFGPass());
    //mPassManager.add(llvm::createLoopRotatePass());
    //mPassManager.add(llvm::createLICMPass());

    //mPassManager.add(llvm::createCFGSimplificationPass());
    //mPassManager.add(llvm::createInstructionCombiningPass());
    mPassManager.add(llvm::createIndVarSimplifyPass());
    mPassManager.add(llvm::createLoopDeletionPass());

    //mPassManager.add(llvm::createNewGVNPass());
}

void LLVMFrontend::registerLateOptimizations()
{
    if (mSettings.optimize) {
        mPassManager.add(llvm::createBasicAAWrapperPass());
        mPassManager.add(llvm::createLICMPass());
    }

    mPassManager.add(llvm::createGlobalOptimizerPass());
    mPassManager.add(llvm::createGlobalDCEPass());

    // Currently loop simplify must be applied for ModuleToAutomata
    // to work properly as it relies on loop preheaders being available.
    mPassManager.add(llvm::createCFGSimplificationPass());
    mPassManager.add(llvm::createLoopSimplifyPass());
    mPassManager.add(gazer::createCanonizeLoopExitsPass());
}

