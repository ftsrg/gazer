#include <gazer/LLVM/Transform/BoundedUnwindPass.h>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>

#include <llvm/Analysis/CFGPrinter.h>

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Scalar/LoopUnrollPass.h>
#include <llvm/Transforms/Scalar.h>

#include <string>

using namespace gazer;

int main(int argc, char* argv[])
{
    if (argc != 3) {
        llvm::errs() << "USAGE: gazer-bmc <input> <bound>";
        return 1;
    }

    std::string input = argv[1];
    unsigned bound = atoi(argv[2]);

    llvm::LLVMContext context;
    llvm::SMDiagnostic err;
    auto module = llvm::parseIRFile(input, err, context);
    
    if (!module) {
        err.print("llvm2cfa", llvm::errs());
        return 1;
    }

    auto pm = std::make_unique<llvm::legacy::PassManager>();

    pm->add(llvm::createPromoteMemoryToRegisterPass());
    pm->add(llvm::createLoopRotatePass());
    //pm->add(llvm::createLoopSimplifyPass());
    //pm->add(new llvm::LoopInfoWrapperPass());
    pm->add(new gazer::BoundedUnwindPass(bound));
    pm->add(llvm::createCFGPrinterLegacyPassPass());

    pm->run(*module);

    return 0;
}