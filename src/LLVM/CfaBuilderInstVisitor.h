#ifndef _GAZER_LLVM_CFABUILDERINSTVISITOR_H
#define _GAZER_LLVM_CFABUILDERINSTVISITOR_H

namespace gazer
{

class CfaBuilderVisitor : public llvm::InstVisitor<CfaBuilderVisitor>
{
public:
    CfaBuilderVisitor(Function& function)
        : mFunction(function), mCFA(new Automaton("entry", "exit")),
        mExprBuilder(CreateExprBuilder())
    {
        mErrorLoc = &mCFA->createLocation("error");
    }

    std::unique_ptr<Automaton> transform();
    void transformBlock(BasicBlock& bb);
    ExprPtr operand(const Value* value);
    Variable* getVariable(const Value* value);

    void insertPHIAssignments(
        std::vector<AssignEdge::Assignment>& assigns,
        const BasicBlock& source,
        const BasicBlock& target
    );

    ExprPtr asBool(ExprPtr operand);
    ExprPtr asInt(ExprPtr operand, unsigned bits);
    ExprPtr castResult(ExprPtr expr, const Type& type);

public:
    void visitBinaryOperator(llvm::BinaryOperator &binop);
    void visitBranchInst(llvm::BranchInst& br);
    //void visitSwitchInst(llvm::SwitchInst& swi);
    void visitSelectInst(llvm::SelectInst& select);
    void visitICmpInst(llvm::ICmpInst& icmp);
    void visitCastInst(llvm::CastInst& cast);
    void visitCallInst(llvm::CallInst& call);
    void visitReturnInst(llvm::ReturnInst& ret);
    void visitInstruction(llvm::Instruction &instr);
    void visitPHINode(llvm::PHINode& phi);
    void visitUnreachableInst(llvm::UnreachableInst& unreachable);

private:
    llvm::Function& mFunction;
    std::unique_ptr<Automaton> mCFA;
    Location* mErrorLoc;
    llvm::DenseMap<const llvm::Value*, Variable*> mVariables;
    llvm::DenseMap<const llvm::BasicBlock*, std::pair<Location*, Location*>> mLocations;
    std::pair<Location*, Location*> mCurrentLocs;
    std::vector<AssignEdge::Assignment> mInstructions;
    bool mBlockReturned = false;
    std::unique_ptr<ExprBuilder> mExprBuilder;
};

} // end namespace gazer

#endif
