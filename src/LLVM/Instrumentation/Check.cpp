#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace gazer;
using namespace llvm;


bool Check::runOnModule(llvm::Module& module)
{
    bool changed = false;

    for (llvm::Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        // Find all the markings
        changed |= this->mark(function);
    }

    return changed;
}


llvm::BasicBlock* Check::createErrorBlock(
    Function& function, llvm::Value* errorCode, const Twine& name)
{
    IRBuilder<> builder(function.getContext());
    BasicBlock* errorBB = BasicBlock::Create(
        function.getContext(), name, &function
    );
    builder.SetInsertPoint(errorBB);
    builder.CreateCall(
        CheckRegistry::GetErrorFunction(function.getParent()),
        { errorCode }
    );
    builder.CreateUnreachable();

    return errorBB;
}

CheckRegistry CheckRegistry::Instance;


llvm::FunctionType* CheckRegistry::GetErrorFunctionType(llvm::LLVMContext& context)
{
    return llvm::FunctionType::get(
        llvm::Type::getVoidTy(context),
        { llvm::Type::getInt16Ty(context) }
    );
}

llvm::Constant* CheckRegistry::GetErrorFunction(llvm::Module& module)
{
    return module.getOrInsertFunction(
        "gazer.error_code",
        GetErrorFunctionType(module.getContext())
    );
}

void CheckRegistry::add(Check* check)
{
    unsigned ec = mErrorCodeCnt++;
    const void* pID = check->getPassID();
    mErrorCodes[pID] = ec;
    mCheckMap[ec] = check;
    mChecks.push_back(check);
}

void CheckRegistry::registerPasses(llvm::legacy::PassManager& pm)
{
    for (Check* check : mChecks) {
        pm.add(check);
    }
}

std::string CheckRegistry::messageForCode(unsigned ec)
{
    auto result = mCheckMap.find(ec);
    assert(result != mCheckMap.end() && "Error code should be present in the check map");

    return result->second->getErrorName();
}

using namespace llvm::PatternMatch;

namespace
{

bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer_error";
}

/**
 * This check ensures that no assertion failure instructions are reachable.
 */
class AssertionFailCheck final : public Check
{
public:
    static char ID;

    AssertionFailCheck()
        : Check(ID)
    {}
    
    bool mark(llvm::Function& function) override
    {
        auto& context = function.getContext();

        BasicBlock* errorBB = this->createErrorBlock(
            function,
            CheckRegistry::GetInstance().getErrorCodeValue(context, ID),
            "error.assert_fail"
        );

        for (BasicBlock& bb : function) {
            if (&bb == errorBB) {
                continue;
            }

            auto it = bb.begin();
            while (it != bb.end()) {
                if (it->getOpcode() == llvm::Instruction::Call) {
                    auto call = llvm::dyn_cast<llvm::CallInst>(&*it);
                    llvm::Function* callee = call->getCalledFunction();

                    // Replace error calls with an uncondiitonal jump
                    // to the error block
                    if (isErrorFunctionName(callee->getName())) {
                        llvm::ReplaceInstWithInst(
                            bb.getTerminator(),
                            llvm::BranchInst::Create(errorBB)
                        );
                        call->eraseFromParent();

                        break;
                    }
                }

                ++it;
            }
        }

        // TODO: Try to preserve some analyses?
        return true;
    }

    std::string getErrorName() const override { return "Assertion failure"; }
};

bool isDiv(unsigned opcode)
{
    return opcode == Instruction::SDiv || opcode == Instruction::UDiv;

}

/**
 * Checks for division by zero errors.
 */
class DivisionByZeroCheck final : public Check
{
public:
    static char ID;

    DivisionByZeroCheck()
        : Check(ID)
    {}

    bool mark(llvm::Function& function) override
    {
        auto& context = function.getContext();

        std::vector<llvm::Instruction*> divs;
        for (llvm::Instruction& inst : instructions(function)) {
            if (isDiv(inst.getOpcode())) {
                divs.push_back(&inst);
            }
        }

        if (divs.empty()) {
            return false;
        }

        BasicBlock* errorBB = this->createErrorBlock(
            function,
            CheckRegistry::GetInstance().getErrorCodeValue(context, ID),
            "error.divzero"
        );

        IRBuilder<> builder(context);
        for (llvm::Instruction* inst : divs) {
            BasicBlock* bb = inst->getParent();
            llvm::Value* rhs = inst->getOperand(1);

            builder.SetInsertPoint(bb);
            auto icmp = builder.CreateICmpNE(
                rhs, builder.getInt(llvm::APInt(
                    rhs->getType()->getIntegerBitWidth(), 0
                ))
            );

            BasicBlock* newBB = inst->getParent()->splitBasicBlock(inst);
            builder.ClearInsertionPoint();
            llvm::ReplaceInstWithInst(
                bb->getTerminator(),
                builder.CreateCondBr(icmp, newBB, errorBB)
            );
        }

        return true;
    }

    std::string getErrorName() const override { return "Divison by zero"; }
};

char DivisionByZeroCheck::ID;
char AssertionFailCheck::ID;

} // end anonymous namespace

Check* gazer::checks::CreateDivisionByZeroCheck() {
    return new DivisionByZeroCheck();
}

Check* gazer::checks::CreateAssertionFailCheck() {
    return new AssertionFailCheck();
}
