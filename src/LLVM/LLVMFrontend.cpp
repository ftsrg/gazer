#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Transform/UndefToNondet.h"
#include "gazer/Trace/TraceWriter.h"
#include "gazer/LLVM/Trace/TestHarnessGenerator.h"

#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Support/CommandLine.h>

#include <llvm/Analysis/CFGPrinter.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Bitcode/BitcodeWriter.h>

#include <boost/algorithm/string/predicate.hpp>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    extern llvm::cl::opt<bool> PrintTrace;
} // end namespace gazer

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

    cl::opt<std::string> TestHarnessFile(
        "test-harness",
        cl::desc("Write test harness to output file"),
        cl::value_desc("filename"),
        cl::init("")
    );

    class RunVerificationBackendPass : public llvm::ModulePass
    {
    public:
        static char ID;

        RunVerificationBackendPass(const CheckRegistry& checks, VerificationAlgorithm& algorithm)
            : ModulePass(ID), mChecks(checks), mAlgorithm(algorithm)
        {}

        void getAnalysisUsage(llvm::AnalysisUsage& au) const override
        {
            au.addRequired<ModuleToAutomataPass>();
            au.setPreservesCFG();
        }

        bool runOnModule(llvm::Module& module) override;

    private:
        const CheckRegistry& mChecks;
        VerificationAlgorithm& mAlgorithm;
        std::unique_ptr<VerificationResult> mResult;
    };

} // end anonymous namespace

char RunVerificationBackendPass::ID;

LLVMFrontend::LLVMFrontend(
    std::unique_ptr<llvm::Module> module,
    GazerContext& context,
    LLVMFrontendSettings settings
)
    : mContext(context),
    mModule(std::move(module)),
    mChecks(mModule->getContext()),
    mSettings(settings)
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
    mPassManager.add(new gazer::ModuleToAutomataPass(mContext, mSettings));

    // 9) Execute the verifier backend if there is one.
    if (mBackendAlgorithm != nullptr) {
        mPassManager.add(new RunVerificationBackendPass(mChecks, *mBackendAlgorithm));
    }
}

bool RunVerificationBackendPass::runOnModule(llvm::Module& module)
{
    ModuleToAutomataPass& moduleToCfa = getAnalysis<ModuleToAutomataPass>();

    AutomataSystem& system = moduleToCfa.getSystem();
    CfaToLLVMTrace cfaToLlvmTrace = moduleToCfa.getTraceInfo();
    LLVMTraceBuilder traceBuilder{system.getContext(), cfaToLlvmTrace};

    mResult = mAlgorithm.check(system, traceBuilder);

    if (auto fail = llvm::dyn_cast<FailResult>(mResult.get())) {
        unsigned ec = fail->getErrorID();
        std::string msg = mChecks.messageForCode(ec);

        llvm::outs() << "Verification FAILED.\n";
        llvm::outs() << "  " << msg << "\n";

        if (PrintTrace) {
            auto writer = trace::CreateTextWriter(llvm::outs(), true);
            llvm::outs() << "Error trace:\n";
            llvm::outs() << "-----------\n";
            writer->write(fail->getTrace());
        }

        if (TestHarnessFile != "") {
            llvm::outs() << "Generating test harness.\n";
            auto test = GenerateTestHarnessModuleFromTrace(
                fail->getTrace(), 
                module.getContext(),
                module
            );

            llvm::StringRef filename(TestHarnessFile);
            std::error_code ec;
            llvm::raw_fd_ostream testOS(filename, ec, llvm::sys::fs::OpenFlags::OF_None);

            if (filename.endswith("ll")) {
                testOS << *test;
            } else {
                llvm::WriteBitcodeToFile(*test, testOS);
            }
        }
    } else if (mResult->isSuccess()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    } else {
        llvm::outs() << "Verification UNKNOWN.\n";
    }

    return false;
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
    llvm::LLVMContext& llvmContext,
    LLVMFrontendSettings settings
) -> std::unique_ptr<LLVMFrontend>
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

    return std::make_unique<LLVMFrontend>(std::move(module), context, settings);
}
