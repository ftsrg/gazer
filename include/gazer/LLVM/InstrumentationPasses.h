#ifndef _GAZER_LLVM_INSTRUMENTATIONPASSES_H
#define _GAZER_LLVM_INSTRUMENTATIONPASSES_H

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>

namespace gazer
{

class Instrumentation
{
public:
    static llvm::Function* getFunctionEntryIntrinsic(llvm::Module& module);
    static llvm::Function* getGazerMallocIntrinsic(llvm::Module& module);
};

llvm::Pass* createPromoteUndefsPass();

llvm::Pass* createMarkFunctionEntriesPass();

llvm::Pass* createInsertLastAddressPass();

}

#endif
