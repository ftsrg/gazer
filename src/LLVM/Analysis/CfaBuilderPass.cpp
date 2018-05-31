#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/Core/Automaton.h"
#include "gazer/Core/Type.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/InstVisitor.h>


using namespace gazer;
using namespace llvm;

namespace
{

gazer::Type& TypeFromLLVMType(llvm::Type* type)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return *BoolType::get();
        }

        return *IntType::get(type->getIntegerBitWidth());
    }

    assert(false && "Unsupported LLVM type.");
}

class CfaBuilderVisitor : public InstVisitor<CfaBuilderVisitor>
{
public:
    CfaBuilderVisitor(Function& function)
        : mFunction(function), mCFA(new Automaton("entry", "exit"))
    {
        mErrorLoc = &mCFA->createLocation("error");
    }

    void transform();

public:
    void visitBinaryOperator(BinaryOperator &binop);
    void visitReturnInst(ReturnInst& ret);
    void visitBranchInst(BranchInst& br);
    void visitSwitchInst(SwitchInst& swi);
    void visitUnreachableInst(UnreachableInst& unreachable);
    void visitICmpInst(ICmpInst& icmp);
    void visitAllocaInst(AllocaInst& alloc);
    void visitLoadInst(LoadInst& load);
    void visitStoreInst(StoreInst& store);
    void visitGetElementPtrInst(GetElementPtrInst& gep);
    void visitPHINode(PHINode& phi);
    void visitCastInst(CastInst& cast);
    void visitSelectInst(SelectInst& select);
    void visitCallInst(CallInst& call);
    void visitInstruction(Instruction &instr);
    void visitFCmpInst(FCmpInst& fcmp);

private:
    Function& mFunction;
    Automaton* mCFA;
    Location* mErrorLoc;
    DenseMap<const Value*, gazer::Variable*> mVariables;
    DenseMap<const BasicBlock*, std::pair<Location*, Location*>> mLocations;
};

void CfaBuilderVisitor::transform()
{
    // Add arguments as local variables
    for (auto& arg : mFunction.args()) {
        Variable& variable = mCFA->getSymbols().create(
            arg.getName(),
            TypeFromLLVMType(arg.getType())
        );
        mVariables[&arg] = &variable;
    }

    for (auto& bb : mFunction) {
        Location* bbEntry = &mCFA->createLocation();
        Location* bbExit = &mCFA->createLocation();

        mLocations[&bb] = std::make_pair(bbEntry, bbExit);

        for (auto& instr : bb) {
            if (instr.getName() != "") {
                Variable& variable = mCFA->getSymbols().create(
                    instr.getName(),
                    TypeFromLLVMType(instr.getType())
                );
                mVariables[&instr] = &variable;
            }
        }
    }

    // Perform the transformation
}

} // end anonymous namespace

bool CfaBuilderPass::runOnFunction(Function& function)
{
    Automaton cfa("entry", "exit");

    for (auto& bb : function) {

    }

    return false;
}
