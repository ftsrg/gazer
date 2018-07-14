#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/Core/Automaton.h"
#include "gazer/Core/Type.h"
#include "gazer/Core/Utils/ExprBuilder.h"
#include "gazer/Core/Utils/CfaSimplify.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

#include <sstream>
#include <algorithm>

using namespace gazer;

using llvm::Instruction;
using llvm::BasicBlock;
using llvm::Function;
using llvm::Value;
using llvm::Argument;
using llvm::GlobalVariable;
using llvm::ConstantInt;
using llvm::isa;
using llvm::dyn_cast;

char CfaBuilderPass::ID = 0;

namespace
{

gazer::Type& TypeFromLLVMType(const llvm::Value* value)
{
    if (value->getType()->isIntegerTy()) {
        auto width = value->getType()->getIntegerBitWidth();
        if (width == 1) {
            return *BoolType::get();
        }

        return *IntType::get(width);
    }

    assert(false && "Unsupported LLVM type.");
}

bool isUnconditionalError(const std::string& name) {
    return name == "__assert_fail" || name == "__VERIFIER_error";
}

bool isLogicInstruction(unsigned opcode) {
    return opcode == Instruction::And || opcode == Instruction::Or || opcode == Instruction::Xor;
}

bool isNonConstValue(const llvm::Value* value) {
    return isa<Instruction>(value) || isa<Argument>(value) || isa<GlobalVariable>(value);
}

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

std::unique_ptr<Automaton> CfaBuilderVisitor::transform()
{
    // Add arguments as local variables
    for (auto& arg : mFunction.args()) {
        Variable& variable = mCFA->getSymbols().create(
            arg.getName(),
            TypeFromLLVMType(&arg)
        );
        mVariables[&arg] = &variable;
    }

    for (auto& bb : mFunction) {
        std::string bbName = bb.getValueName()->first();

        Location* bbEntry = &mCFA->createLocation(bbName + ".entry");
        Location* bbExit = &mCFA->createLocation(bbName + ".exit");

        mLocations[&bb] = std::make_pair(bbEntry, bbExit);

        for (auto& instr : bb) {
            if (instr.getName() != "") {
                Variable& variable = mCFA->getSymbols().create(
                    instr.getName(),
                    TypeFromLLVMType(&instr)
                );
                mVariables[&instr] = &variable;
            }
        }
    }

    // Add a transition from the initial location
    auto initLoc = mLocations[&(mFunction.getEntryBlock())].first;
    mCFA->insertEdge(AssignEdge::Create(mCFA->entry(), *initLoc));

    // Perform the transformation
    for (auto& bb : mFunction) {
        mBlockReturned = false;
        mCurrentLocs = mLocations[&bb];
        transformBlock(bb);
        
        if (!mBlockReturned) {
            mCFA->insertEdge(
                AssignEdge::Create(*mCurrentLocs.first, *mCurrentLocs.second, mInstructions)
            );
        }
    }

    return std::move(mCFA);
}

ExprPtr CfaBuilderVisitor::operand(const Value* value)
{
    // Check for boolean literals
    if (const ConstantInt* ci = dyn_cast<ConstantInt>(value)) {
        if (ci->getType()->isIntegerTy(1)) {
            return ci->isZero() ? mExprBuilder->False() : mExprBuilder->True();
        }

        return mExprBuilder->IntLit(
            ci->getValue().getLimitedValue(),
            ci->getType()->getIntegerBitWidth()
        );
    } else if (isNonConstValue(value)) {
        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder->Undef(TypeFromLLVMType(value));
    } else {
        assert(false && "Unhandled value type");
    }
}

Variable* CfaBuilderVisitor::getVariable(const Value* value)
{
    auto result = mVariables.find(value);
    assert(result != mVariables.end() && "Variables should present in the variable map.");

    return result->second;
}

void CfaBuilderVisitor::transformBlock(llvm::BasicBlock& bb)
{
    mInstructions.clear();
    visit(bb);
}

ExprPtr CfaBuilderVisitor::asBool(ExprPtr operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    } else if (operand->getType().isIntType()) {
        const IntType* intType = dyn_cast<IntType>(&operand->getType());
        unsigned bits = intType->getWidth();

        return mExprBuilder->Select(
            mExprBuilder->Eq(operand, mExprBuilder->IntLit(0, bits)),
            mExprBuilder->False(),
            mExprBuilder->True()
        );
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr CfaBuilderVisitor::asInt(ExprPtr operand, unsigned bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder->Select(
            operand,
            mExprBuilder->IntLit(1, bits),
            mExprBuilder->IntLit(0, bits)
        );
    } else if (operand->getType().isIntType()) {
        return operand;
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr CfaBuilderVisitor::castResult(ExprPtr expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isIntType()) {
        return asInt(expr, dyn_cast<IntType>(&type)->getWidth());
    } else {
        throw TypeCastError("Invalid cast result type.");
    }
}

void CfaBuilderVisitor::insertPHIAssignments(
    std::vector<AssignEdge::Assignment>& instructions,
    const BasicBlock& source,
    const BasicBlock& target)
{
    for (auto it = target.begin(); it != target.getFirstInsertionPt(); ++it) {
        if (it->getOpcode() == Instruction::PHI) {
            const llvm::PHINode *phi = dyn_cast<llvm::PHINode>(&*it);

            int bbIdx = phi->getBasicBlockIndex(&source);
            if (bbIdx != -1) {
                // If the current block is present in this PHI node
                Value *value = phi->getIncomingValue(bbIdx);
                ExprPtr   expr = operand(value);
                Variable* phiVar  = getVariable(phi);

                instructions.push_back({phiVar, expr});
            }
        }
    }
}

void CfaBuilderVisitor::visitBinaryOperator(llvm::BinaryOperator& binop)
{
    auto variable = getVariable(&binop);
    auto lhs = operand(binop.getOperand(0));
    auto rhs = operand(binop.getOperand(1));

    auto opcode = binop.getOpcode();
    if (isLogicInstruction(opcode)) {
        ExprPtr expr;
        if (binop.getType()->isIntegerTy(1)) {
            auto boolLHS = asBool(lhs);
            auto boolRHS = asBool(rhs);

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->Xor(boolLHS, boolRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        } else {
            assert(binop.getType()->isIntegerTy()
                && "Integer operations on non-integer types");
            auto iTy = llvm::dyn_cast<llvm::IntegerType>(binop.getType());

            auto intLHS = asInt(lhs, iTy->getBitWidth());
            auto intRHS = asInt(rhs, iTy->getBitWidth());

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->BAnd(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->BOr(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->BXor(intLHS, intRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        }

        mInstructions.push_back({variable, expr});
    } else {
        const IntType* type = llvm::dyn_cast<IntType>(&variable->getType());
        assert(type && "Arithmetic results must be integer types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

        #define HANDLE_INSTCASE(OPCODE, EXPRNAME)                 \
            case OPCODE:                                          \
                expr = mExprBuilder->EXPRNAME(intLHS, intRHS);    \
                break;                                            \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add, Add)
            HANDLE_INSTCASE(Instruction::Sub, Sub)
            HANDLE_INSTCASE(Instruction::Mul, Mul)
            HANDLE_INSTCASE(Instruction::Shl, Shl)
            HANDLE_INSTCASE(Instruction::LShr, LShr)
            HANDLE_INSTCASE(Instruction::AShr, AShr)
            default:
                assert(false && "Unsupported arithmetic instruction opcode");
        }

        #undef HANDLE_INSTCASE

        mInstructions.push_back({variable, expr});
    }
}

void CfaBuilderVisitor::visitBranchInst(llvm::BranchInst& br)
{
    BasicBlock* fromBB = br.getParent();
    BasicBlock* thenBB = br.getSuccessor(0);
    Location* thenLoc = mLocations[thenBB].first;

    if (br.isUnconditional()) {
        std::vector<AssignEdge::Assignment> assigns;
        insertPHIAssignments(assigns, *fromBB, *thenBB);
        mCFA->insertEdge(AssignEdge::Create(*mCurrentLocs.second, *thenLoc, assigns));
    } else {
        BasicBlock* elzeBB = br.getSuccessor(1);
        Location* elzeLoc = mLocations[elzeBB].first;

        ExprPtr condition = asBool(operand(br.getCondition()));

        std::vector<AssignEdge::Assignment> thenAssigns;
        insertPHIAssignments(thenAssigns, *fromBB, *thenBB);
        
        std::vector<AssignEdge::Assignment> elzeAssigns;
        insertPHIAssignments(elzeAssigns, *fromBB, *thenBB);

        mCFA->insertEdge(AssignEdge::Create(
            *mCurrentLocs.second, *thenLoc, thenAssigns, condition
        ));
        mCFA->insertEdge(AssignEdge::Create(
            *mCurrentLocs.second, *elzeLoc, elzeAssigns, mExprBuilder->Not(condition) 
        ));
    }
}

#if 0
void CfaBuilderVisitor::visitSwitchInst(llvm::SwitchInst& swi)
{
    auto condition = operand(swi.getCondition());
    llvm::DenseMap<const BasicBlock*, Location*> cases;

    for (auto& ci : swi.cases()) {
        const BasicBlock* target = ci.getCaseSuccessor();
        const Value* value = ci.getCaseValue();

        auto valueOp = operand(value);
        auto jumpCondition = EqExpr::Create(condition, valueOp);

        auto targetLoc = mLocations[target].first;
        mCFA->insertEdge(AssignEdge::Create(
            *mCurrentLocs.second,
            *targetLoc,
            jumpCondition
        ));
    }
}
#endif

void CfaBuilderVisitor::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    mInstructions.push_back({selectVar, mExprBuilder->Select(cond, then, elze)});
}

void CfaBuilderVisitor::visitICmpInst(llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;
    
    auto icmpVar = getVariable(&icmp);
    auto lhs = operand(icmp.getOperand(0));
    auto rhs = operand(icmp.getOperand(1));

    auto pred = icmp.getPredicate();

    #define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                \
        case PREDNAME:                                          \
            expr = mExprBuilder->EXPRNAME(lhs, rhs);            \
            break;                                              \

    ExprPtr expr;
    switch (pred) {
        HANDLE_PREDICATE(CmpInst::ICMP_EQ, Eq)
        HANDLE_PREDICATE(CmpInst::ICMP_NE, NotEq)
        HANDLE_PREDICATE(CmpInst::ICMP_UGT, UGt)
        HANDLE_PREDICATE(CmpInst::ICMP_UGE, UGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_ULT, ULt)
        HANDLE_PREDICATE(CmpInst::ICMP_ULE, ULtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SGT, SGt)
        HANDLE_PREDICATE(CmpInst::ICMP_SGE, SGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SLT, SLt)
        HANDLE_PREDICATE(CmpInst::ICMP_SLE, SLtEq)
        default:
            assert(false && "Unhandled ICMP predicate.");
    }

    #undef HANDLE_PREDICATE

    mInstructions.push_back({icmpVar, expr});
}

void CfaBuilderVisitor::visitCastInst(llvm::CastInst& cast)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::IntType>(&variable->getType());

    auto castOp = operand(cast.getOperand(0));
    
    unsigned width = 0;
    if (castOp->getType().isBoolType()) {
        width = 1;        
    } else if (castOp->getType().isIntType()) {
        width = dyn_cast<IntType>(&castOp->getType())->getWidth();
    } else {
        assert(false && "Unsupported cast type.");
    }

    auto intOp = asInt(castOp, width);

    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder->ZExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder->SExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::Trunc) {
        castOp = mExprBuilder->Trunc(intOp, *intTy);
    } else {
        // Other cast types are ignored at the moment.
    }

    mInstructions.push_back({variable, castOp});
}

void CfaBuilderVisitor::visitCallInst(llvm::CallInst& call)
{
    const Function* callee = call.getCalledFunction();
    assert(callee != nullptr && "Indirect calls are not supported.");

    if (isUnconditionalError(callee->getName())) {
        // Split the block here
        mBlockReturned = true;
        mCFA->insertEdge(
            AssignEdge::Create(*mCurrentLocs.first, *mErrorLoc, mInstructions)
        );
    } else if (callee->isDeclaration()) {
        // If this function has no definition,
        // we just replace the call with a Havoc statement
        if (call.getName() != "") {
            auto variable = getVariable(&call);
            mInstructions.push_back(
                {variable, UndefExpr::Get(variable->getType())}
            );
        }
    } else {
        assert(false && "Procedure calls are not supported (with the exception of assert).");
    }
}

void CfaBuilderVisitor::visitReturnInst(llvm::ReturnInst& ret)
{
    mCFA->insertEdge(AssignEdge::Create(*mCurrentLocs.second, mCFA->exit()));
    mCFA->insertEdge(AssignEdge::Create(*mCurrentLocs.first, *mCurrentLocs.second, mInstructions));
    mBlockReturned = true;
}

// Ignore these for now.
void CfaBuilderVisitor::visitUnreachableInst(llvm::UnreachableInst& unreachable)
{}

// Intentionally empty.
// PHI nodes are handled on the branching instructions leading to them.
void CfaBuilderVisitor::visitPHINode(llvm::PHINode& phi)
{}

void CfaBuilderVisitor::visitInstruction(llvm::Instruction &instr)
{
    llvm::errs() << "Unhandled instruction: ";
    instr.print(llvm::errs());
    llvm::errs() << "\n";
}

} // end anonymous namespace


bool CfaBuilderPass::runOnFunction(Function& function)
{
    CfaBuilderVisitor visitor(function);

    mCFA = visitor.transform();
    if (mLBE) {
        SimplifyCFA(*mCFA);
    }

    llvm::errs() << "FINISHED BUILDING AUTOMATON. STATS:\n";
    llvm::errs() << "   Encoding: " << (mLBE ? "LargeBlock" : "SmallBlock") << "\n";
    llvm::errs() << "   Locations: " << mCFA->getNumLocs() << "\n";
    llvm::errs() << "   Edges: " << mCFA->getNumEdges() << "\n";
    llvm::errs() << "   Variables: " << mCFA->getSymbols().size() << "\n";
    //printAutomaton(*mCFA);
    
    return false;
}

namespace
{

void printAutomaton(Automaton& cfa)
{
    llvm::errs() << "digraph G {\n";
    for (auto& loc : cfa.locs()) {
        llvm::errs() << fmt::format("\tnode_{0} [label=\"{1}\"];\n",
            static_cast<void*>(loc.get()), loc->getName()
        );
    }
    llvm::errs() << "\n";
    for (auto& edge : cfa.edges()) {
        std::stringstream ss;
        edge->print(ss);

        llvm::errs() << fmt::format("\tnode_{0} -> node_{1} [label=\"{2}\"];\n",
            static_cast<void*>(&edge->getSource()),
            static_cast<void*>(&edge->getTarget()),
            ss.str()
        );
    }
    llvm::errs() << "};\n";
}

struct CfaPrinterPass final : public llvm::FunctionPass
{
    static char ID;

    CfaPrinterPass()
        : FunctionPass(ID)
    {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override {
        au.addRequired<CfaBuilderPass>();
        au.setPreservesAll();
    }

    virtual bool runOnFunction(llvm::Function& function) override {
        Automaton& cfa = getAnalysis<CfaBuilderPass>().getCFA();
        printAutomaton(cfa);

        return false;
    }
};

char CfaPrinterPass::ID;

} // end anonymous namespace

namespace gazer {
    llvm::Pass* createCfaBuilderPass(bool isLBE) { return new CfaBuilderPass(isLBE); }
    llvm::Pass* createCfaPrinterPass() { return new CfaPrinterPass(); }
}

