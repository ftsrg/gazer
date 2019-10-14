#ifndef GAZER_LLVM_CLANGFRONTEND_H
#define GAZER_LLVM_CLANGFRONTEND_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>

namespace llvm
{
    class Module;
    class LLVMContext;
}

namespace gazer
{

/// Compiles a set of C and/or LLVM bitcode files using clang, links them
/// together with llvm-link and parses the resulting module.
std::unique_ptr<llvm::Module> ClangCompileAndLink(
    llvm::ArrayRef<std::string> files,
    llvm::LLVMContext& llvmContext
);

}

#endif //GAZER_CLANGFRONTEND_H
