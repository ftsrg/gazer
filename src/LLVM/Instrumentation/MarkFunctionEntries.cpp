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
#include "gazer/LLVM/InstrumentationPasses.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>

#include <llvm/Pass.h>

using namespace gazer;
using namespace llvm;

namespace
{

class MarkFunctionEntriesPass : public ModulePass
{
public:
    static char ID;

    MarkFunctionEntriesPass()
        : ModulePass(ID)
    {}

    llvm::StringRef getPassName() const override {
        return "Mark function entries";
    }

    bool runOnModule(Module& module) override
    {
        LLVMContext& context = module.getContext();
        llvm::DenseMap<llvm::Type*, llvm::Value*> returnValueMarks;

        auto retMarkVoid = GazerIntrinsic::GetOrInsertFunctionReturnVoid(module);        
        auto callReturnedMark = GazerIntrinsic::GetOrInsertFunctionCallReturned(module);

        IRBuilder<> builder(context);
        for (Function& function : module) {
            if (function.isDeclaration()) {
                continue;
            }

            BasicBlock& entry = function.getEntryBlock();
            entry.splitBasicBlock(entry.begin());

            auto dsp = function.getSubprogram();
            if (dsp != nullptr) {
                auto mark = GazerIntrinsic::GetOrInsertFunctionEntry(module, function.getFunctionType()->params());
                builder.SetInsertPoint(&entry, entry.getFirstInsertionPt());

                std::vector<llvm::Value*> args;
                args.push_back(MetadataAsValue::get(context, dsp));
                for (auto& arg : function.args()) {
                    args.push_back(&arg);
                }

                builder.CreateCall(mark, args);
            } else {
                llvm::errs()
                    << "Cannot insert function entry marks: "
                    << "DISubprogram missing.\n";
                continue;
            }

            // Also mark call returns to other functions
            std::vector<ReturnInst*> returns;
            for (Instruction& inst : llvm::instructions(function)) {
                if (auto ret = dyn_cast<ReturnInst>(&inst)) {
                    returns.push_back(ret);
                }
            }

            for (ReturnInst* ret : returns) {
                builder.SetInsertPoint(ret);
                llvm::Value* retValue = ret->getReturnValue();
                if (retValue != nullptr) {
                    auto retValueTy = retValue->getType();
                    llvm::Value* retMark = returnValueMarks[retValueTy];
                    if (retMark == nullptr) {
                        std::string nameBuffer;
                        llvm::raw_string_ostream rso(nameBuffer);
                        retValueTy->print(rso, false, true);
                        rso.flush();

                        // Insert a new function for this mark type
                        retMark = GazerIntrinsic::GetOrInsertFunctionReturnValue(module, retValueTy).getCallee();
                        returnValueMarks[retValueTy] = retMark;
                    }

                    builder.CreateCall(retMark, {
                        MetadataAsValue::get(context, dsp),
                        retValue
                    });
                } else {
                    builder.CreateCall(retMarkVoid, {
                        MetadataAsValue::get(context, dsp)
                    });
                }
            }

            // Also mark call returns from other functions
            std::vector<CallInst*> calls;
            for (Instruction& inst : llvm::instructions(function)) {
                if (auto call = dyn_cast<CallInst>(&inst)) {
                    Function* callee = call->getCalledFunction();
                    if (callee == nullptr || callee->isDeclaration()) {
                        // Currently we do not bother with indirect calls,
                        // also we only need functions which have a definition.
                        continue;
                    }

                    calls.push_back(call);
                }
            }

            for (CallInst* call : calls) {
                builder.SetInsertPoint(call->getNextNode());
                builder.CreateCall(callReturnedMark, {
                    MetadataAsValue::get(context, dsp)
                });
            }
        }

        return true;
    }

};

} // end anonymous namespace

char MarkFunctionEntriesPass::ID = 0;


static llvm::RegisterPass<MarkFunctionEntriesPass> X("gazer-mark-function-entries",
                                                      "Gazer function entry marker pass",
                                                      false /* Only looks at CFG */,
                                                      false /* Analysis Pass */);

namespace gazer {
    llvm::Pass* createMarkFunctionEntriesPass() {
        return new MarkFunctionEntriesPass();
    }
} // end namespace gazer