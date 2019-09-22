#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"

#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Verifier/BmcPass.h"

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Analysis/CFGPrinter.h>

#include <llvm/AsmParser/Parser.h>
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
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>

#include <string>
#include <filesystem>

using namespace gazer;
using namespace llvm;

namespace {
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    cl::opt<bool> RunBmc("bmc", cl::desc("Run Bounded Model Checking"));
    cl::opt<unsigned> BmcUnwind("unwind", cl::desc("Unwind limit for BMC"), cl::init(0));
    cl::opt<bool> PrintCFA("print-cfa", cl::desc("Print the resulting CFA"));
    cl::opt<bool> SimplifyLoops("loop-simplify", cl::desc("Run loop transformation passes"));
    cl::opt<bool> NoOptimize("no-optimize", cl::desc("Run optimization passes"));
    cl::opt<bool> InlineFunctions("inline", cl::desc("Inline function calls."));
    cl::opt<bool> InlineGlobals("inline-globals", cl::desc("Inline global variables"));
    cl::opt<bool> PrintPDG("print-pdg", cl::desc("Print the Program Dependence Graph (PDG)"));
    cl::opt<bool> BackwardSlice("slice", cl::desc("Perform static backward slicing"));
    cl::opt<bool> LargeBlockCFA("lbe-cfa", cl::desc("Transform the CFA to large block encoding"));
    cl::opt<bool> ShowUnrolledCFG("show-unrolled-cfg", cl::desc("Display the unrolled CFG"));
}

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;

    std::string input = InputFilename;
    namespace fs = std::filesystem;

    llvm::LLVMContext llvmContext;
    llvm::SMDiagnostic err;
    std::unique_ptr<llvm::Module> module;

    fs::path inputPath{InputFilename.c_str()};
    if (inputPath.extension() == ".bc") {
        module = llvm::parseIRFile(inputPath.c_str(), err, llvmContext);
    } else if (inputPath.extension() == ".ll") {
        module = llvm::parseAssemblyFile(inputPath.c_str(), err, llvmContext);
    } else {
        llvm::errs() << "ERROR: Input file must be an LLVM bitcode (.bc) or LLVM assembly (.ll) format.\n";
        return 1;
    }

    if (!module) {
        err.print("gazer-bmc", llvm::errs());
        return 1;
    }

    // TODO: This should be more flexible.
    if (module->getFunction("main") == nullptr) {
        llvm::errs() << "ERROR: No 'main' function found.\n";
        return 1;
    }

    GazerContext context;
    auto pm = std::make_unique<llvm::legacy::PassManager>();

    if (PrintCFA) {
        pm->add(new gazer::ModuleToAutomataPass(context));
        pm->add(gazer::createCfaPrinterPass());

        pm->run(*module);
        
        llvm::llvm_shutdown();
        return 0;
    }

    pm->add(llvm::createPromoteMemoryToRegisterPass());

    // Perform error instrumentation
    CheckRegistry checks(llvmContext);
    checks.add(checks::CreateAssertionFailCheck());
    checks.add(checks::CreateDivisionByZeroCheck());
    checks.registerPasses(*pm);

    if (!NoOptimize) {
        pm->add(llvm::createIPSCCPPass());
        pm->add(llvm::createGlobalOptimizerPass());
        pm->add(llvm::createDeadArgEliminationPass());
        pm->add(llvm::createInstructionCombiningPass(true));
        pm->add(llvm::createCFGSimplificationPass());
    }

    // Do the requested inlining early on.
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

        pm->add(gazer::createMarkFunctionEntriesPass());
        pm->add(llvm::createAlwaysInlinerLegacyPass());

        if (InlineGlobals) {
            pm->add(createInlineGlobalVariablesPass());
        }
        pm->add(llvm::createGlobalDCEPass());
    }

    pm->add(llvm::createPromoteMemoryToRegisterPass());

    if (!NoOptimize) {
        pm->add(llvm::createSROAPass());
        pm->add(llvm::createEarlyCSEPass());
        //pm->add(llvm::createLoopRotatePass());
        //pm->add(llvm::createIndVarSimplifyPass());
        //pm->add(llvm::createLoopSimplifyPass());

        pm->add(llvm::createFloat2IntPass());
        //pm->add(llvm::createGVNHoistPass());
        //pm->add(llvm::createGVNSinkPass());
        pm->add(llvm::createInstructionCombiningPass(true));
        //pm->add(llvm::createCFGSimplificationPass());

        pm->add(llvm::createReassociatePass());

        // Loops
        pm->add(llvm::createLoopSimplifyPass());
        pm->add(llvm::createLoopRotatePass());
        pm->add(llvm::createLICMPass());
        pm->add(llvm::createCFGSimplificationPass());
        pm->add(llvm::createInstructionCombiningPass(true));
        pm->add(llvm::createIndVarSimplifyPass());
        pm->add(llvm::createLoopDeletionPass());

        pm->add(llvm::createNewGVNPass());
        pm->add(llvm::createBitTrackingDCEPass());
        pm->add(llvm::createInstructionCombiningPass(true));
        pm->add(llvm::createAggressiveDCEPass());
    }

    pm->add(llvm::createCFGSimplificationPass());
    pm->add(llvm::createLoopSimplifyPass());

    // Perform memory object analysis
    pm->add(new BasicAAWrapperPass());
    pm->add(new GlobalsAAWrapperPass());
    pm->add(new MemorySSAWrapperPass());
    //pm->add(new MemoryObjectPass());

    pm->add(llvm::createInstructionNamerPass());
    pm->add(createCombineErrorCallsPass());

    if (ShowUnrolledCFG) {
        pm->add(llvm::createCFGPrinterLegacyPassPass());
    }

    pm->add(new gazer::ModuleToAutomataPass(context));

    if (RunBmc) {
        pm->add(new gazer::BoundedModelCheckerPass(checks));
    }

    //pm->add(llvm::createCFGPrinterLegacyPassPass());

    pm->run(*module);

    llvm::llvm_shutdown();

    return 0;
}

