#ifndef _GAZER_LLVM_TESTGENERATOR_TESTGENERATOR_H
#define _GAZER_LLVM_TESTGENERATOR_TESTGENERATOR_H

#include "gazer/Trace/Trace.h"

#include <llvm/IR/Module.h>

namespace gazer
{

class TestGenerator
{
public:
    std::unique_ptr<llvm::Module> generateModuleFromTrace(
       Trace& trace, llvm::LLVMContext& context, const llvm::Module& module
    );
};

}

#endif
