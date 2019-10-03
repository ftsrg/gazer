#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Transform/UndefToNondet.h"

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/Analysis/CFGPrinter.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>

#include <boost/algorithm/string/predicate.hpp>

using namespace gazer;
using namespace llvm;

namespace
{    
    cl::opt<bool> InlineFunctions(
        "inline", cl::desc("Inline function calls."));
    cl::opt<bool> InlineGlobals(
        "inline-globals", cl::desc("Inline global variables"));
    
    cl::opt<bool> NoOptimize(
        "no-optimize", cl::desc("Run optimization passes"));
    cl::opt<bool> ShowUnrolledCFG(
        "show-unrolled-cfg", cl::desc("Display the unrolled CFG"));
}

LLVMFrontend::LLVMFrontend(
    std::unique_ptr<llvm::Module> module,
    GazerContext& context
)
    : mContext(context),
    mModule(std::move(module)),
    mChecks(mModule->getContext())
{}

void LLVMFrontend::registerVerificationPipeline()
{
    // 1) Do basic preprocessing: get rid of alloca's and turn undef's
    //  into nondet function calls.
    mPassManager.add(llvm::createPromoteMemoryToRegisterPass());
    mPassManager.add(new gazer::UndefToNondetCallPass());

    // 2) Execute early optimization passes.
    registerEarlyOptimizations();

    // 3) Perform check instrumentation.
    registerEnabledChecks();

    // 4) Inline functions and global variables if requested.
    registerInliningIfEnabled();

    // 5) Execute late optimization passes.
    registerLateOptimizations();

    // 6) Create memory models.
    mPassManager.add(new llvm::BasicAAWrapperPass());
    mPassManager.add(new llvm::GlobalsAAWrapperPass());
    mPassManager.add(new llvm::MemorySSAWrapperPass());

    // 7) Do an instruction namer pass.
    mPassManager.add(llvm::createInstructionNamerPass());

    // Display the final LLVM CFG now.
    if (ShowUnrolledCFG) {
        mPassManager.add(llvm::createCFGPrinterLegacyPassPass());
    }

    // 8) Perform module-to-automata translation.
    mPassManager.add(new gazer::ModuleToAutomataPass(mContext));
}

void LLVMFrontend::registerEnabledChecks()
{
    mChecks.add(checks::CreateAssertionFailCheck());
    mChecks.add(checks::CreateDivisionByZeroCheck());
    mChecks.registerPasses(mPassManager);
}

void LLVMFrontend::registerInliningIfEnabled()
{
    if (InlineFunctions) {
        // Mark all functions but the main as 'always inline'
        for (auto &func : mModule->functions()) {
            // Ignore the main function and declaration-only functions
            if (func.getName() != "main" && !func.isDeclaration()) {
                func.addAttribute(llvm::AttributeList::FunctionIndex, llvm::Attribute::AlwaysInline);
                func.setLinkage(GlobalValue::InternalLinkage);
            }
        }

        // Mark globals as internal
        for (auto &gv : mModule->globals()) {
            gv.setLinkage(GlobalValue::InternalLinkage);
        }

        mPassManager.add(gazer::createMarkFunctionEntriesPass());
        mPassManager.add(llvm::createAlwaysInlinerLegacyPass());

        if (InlineGlobals) {
            mPassManager.add(createInlineGlobalVariablesPass());
        }

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
    if (!NoOptimize) {
        mPassManager.add(llvm::createCFGSimplificationPass());
        mPassManager.add(llvm::createSROAPass());
    
        mPassManager.add(llvm::createIPSCCPPass());
        mPassManager.add(llvm::createGlobalOptimizerPass());
        mPassManager.add(llvm::createDeadArgEliminationPass());
    }
}

void LLVMFrontend::registerLateOptimizations()
{
    if (!NoOptimize) {
        mPassManager.add(llvm::createFloat2IntPass());

        mPassManager.add(llvm::createIndVarSimplifyPass());
        mPassManager.add(llvm::createLoopDeletionPass());
        mPassManager.add(llvm::createLICMPass());
        mPassManager.add(llvm::createLoopUnswitchPass());
    }

    // Currently loop simplify must be applied for ModuleToAutomata
    // to work properly as it relies on loop preheaders being available.
    mPassManager.add(llvm::createCFGSimplificationPass());
    mPassManager.add(llvm::createLoopSimplifyPass());
}

auto LLVMFrontend::FromInputFile(
    llvm::StringRef input,
    GazerContext& context,
    llvm::LLVMContext& llvmContext) -> std::unique_ptr<LLVMFrontend>
{
    llvm::SMDiagnostic err;

    std::unique_ptr<llvm::Module> module = nullptr;

    if (boost::algorithm::ends_with(input, ".bc")) {
        module = llvm::parseIRFile(input, err, llvmContext);
    } else if (boost::algorithm::ends_with(input, ".ll")) {
        module = llvm::parseAssemblyFile(input, err, llvmContext);
    } else {
        err = SMDiagnostic(
            input,
            SourceMgr::DK_Error,
            "Input file must be in LLVM bitcode (.bc) or LLVM assembly (.ll) format."
        );
    }

    if (module == nullptr) {
        err.print(nullptr, llvm::errs());
        return nullptr;
    }

    return std::make_unique<LLVMFrontend>(std::move(module), context);
}
