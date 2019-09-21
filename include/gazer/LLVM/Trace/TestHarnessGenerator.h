#ifndef GAZER_LLVM_TRACE_TESTHARNESSGENERATOR_H
#define GAZER_LLVM_TRACE_TESTHARNESSGENERATOR_H

#include "gazer/Trace/Trace.h"

#include <llvm/IR/Module.h>

namespace gazer
{

std::unique_ptr<llvm::Module> GenerateTestHarnessModuleFromTrace(
    Trace& trace, llvm::LLVMContext& context, const llvm::Module& module
);

}

#endif
