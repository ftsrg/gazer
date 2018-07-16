#include "gazer/LLVM/Transform/BoundedUnwindPass.h"
#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/ProgramDependence.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Analysis/ProgramDependence.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"

#include "gazer/LLVM/InstrumentationPasses.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Analysis/CFGPrinter.h>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Scalar/LoopUnrollPass.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Support/CommandLine.h>

#include <string>

using namespace gazer;
using namespace llvm;

namespace {
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    cl::opt<bool> RunBmc("bmc", cl::desc("Run Bounded Model Checking"));
    cl::opt<unsigned> BmcUnwind("unwind", cl::desc("Unwind limit for BMC"), cl::init(0));
    cl::opt<bool> PrintCFA("print-cfa", cl::desc("Print the resulting CFA"));
    cl::opt<bool> SimplifyLoops("loop-simplify", cl::desc("Run loop transformation passes"));
    cl::opt<bool> Optimize("optimize", cl::desc("Run optimization passes"));
    cl::opt<bool> InlineFunctions("inline", cl::desc("Inline function calls."));
    cl::opt<bool> InlineGlobals("inline-globals", cl::desc("Inline global variables"));
    cl::opt<bool> PrintPDG("print-pdg", cl::desc("Print the Program Dependence Graph (PDG)"));
    cl::opt<bool> BackwardSlice("slice", cl::desc("Perform static backward slicing"));
    cl::opt<bool> LargeBlockCFA("lbe-cfa", cl::desc("Transform the CFA to large block encoding"));
}

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    std::string input = InputFilename;
    unsigned bound = BmcUnwind;

    llvm::LLVMContext context;
    llvm::SMDiagnostic err;
    auto module = llvm::parseIRFile(input, err, context);
    
    if (!module) {
        err.print("llvm2cfa", llvm::errs());
        return 1;
    }

    auto pm = std::make_unique<llvm::legacy::PassManager>();

    if (InlineFunctions) {
        // Mark all functions but the main as 'always inline'
        for (auto &func : module->functions()) {
            // Ignore the main function and declaration-only functions
            if (func.getName() != "main" && !func.isDeclaration()) {
                func.addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AlwaysInline);
                func.setLinkage(GlobalValue::InternalLinkage);
            }
        }

        // Mark globals as internal
        for (auto &gv : module->globals()) {
            gv.setLinkage(GlobalValue::InternalLinkage);
        }

        pm->add(llvm::createAlwaysInlinerLegacyPass());

        if (InlineGlobals) {
            // If -inline-globals is also requested...
            pm->add(createInlineGlobalVariablesPass());
        }
        pm->add(llvm::createGlobalDCEPass());
    }

    if (Optimize) {
        pm->add(llvm::createLoopRotatePass());
        pm->add(llvm::createIndVarSimplifyPass());
        pm->add(llvm::createLICMPass());
        pm->add(llvm::createInstructionCombiningPass(true));
        pm->add(llvm::createReassociatePass());
        pm->add(llvm::createConstantPropagationPass());
        pm->add(llvm::createDeadCodeEliminationPass());
        pm->add(llvm::createCFGSimplificationPass());
        pm->add(llvm::createStructurizeCFGPass());
        pm->add(llvm::createLowerSwitchPass());
    }

    pm->add(llvm::createPromoteMemoryToRegisterPass());
    pm->add(llvm::createInstructionNamerPass());

    bool NeedsPDG = BackwardSlice || PrintPDG;

    if (NeedsPDG) {
        pm->add(llvm::createPostDomTree());
        pm->add(gazer::createProgramDependenceWrapperPass());
    }
    if (PrintPDG) {
        pm->add(gazer::createProgramDependencePrinterPass());
    }
    if (BackwardSlice) {
        pm->add(gazer::createBackwardSlicerPass());
        pm->add(llvm::createVerifierPass());
        pm->add(llvm::createConstantPropagationPass());
        pm->add(llvm::createDeadCodeEliminationPass());
        pm->add(llvm::createCFGSimplificationPass());  
    }

    if (RunBmc) {
        pm->add(llvm::createLoopSimplifyPass());
        pm->add(llvm::createLoopRotatePass());
        pm->add(llvm::createIndVarSimplifyPass());
        pm->add(llvm::createLoopSimplifyPass());
        
        pm->add(new llvm::DominatorTreeWrapperPass());
        //pm->add(new llvm::LoopInfoWrapperPass());
        //pm->add(new llvm::ScalarEvolutionWrapperPass());
        //pm->add(new llvm::AssumptionCacheTracker());
        pm->add(new gazer::BoundedUnwindPass(bound));
        pm->add(gazer::createPromoteUndefsPass());
        //pm->add(llvm::createCFGSimplificationPass());
        pm->add(createTopologicalSortPass());
        //pm->add(llvm::createVerifierPass());
        //pm->add(new gazer::CfaBuilderPass(LargeBlockCFA));
        //if (PrintCFA) {
        //    pm->add(createCfaPrinterPass());
        //}
        //pm->add(llvm::createCFGPrinterLegacyPassPass());
        pm->add(new gazer::BmcPass(bound));
    }

    //pm->add(llvm::createCFGPrinterLegacyPassPass());

    pm->run(*module);

    llvm::llvm_shutdown();

    return 0;
}

