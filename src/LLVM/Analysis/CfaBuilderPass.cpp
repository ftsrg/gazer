#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/Core/Automaton.h"
#include "gazer/Core/Type.h"
#include "gazer/Core/Utils/ExprBuilder.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

#include <sstream>

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
            TypeFromLLVMType(arg.getType())
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
                    TypeFromLLVMType(instr.getType())
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
        return mExprBuilder->Undef(TypeFromLLVMType(value->getType()));
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

                instructions.push_back({*phiVar, expr});
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
        if (binop.getType()->isIntegerTy(1)) {
            auto boolLHS = asBool(lhs);
            auto boolRHS = asBool(rhs);

            ExprPtr expr;
            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->Xor(boolLHS, boolRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }

            mInstructions.push_back({*variable, expr});
        } else {
            assert(false && "Unsupported operator kind.");
        }
    } else {
        const IntType* type = llvm::dyn_cast<IntType>(&variable->getType());
        assert(type && "Arithmetic results must be integer types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

        ExprPtr expr;
        if (binop.getOpcode() == Instruction::Add) {
            expr = mExprBuilder->Add(intLHS,intRHS);
        } else if (binop.getOpcode() == Instruction::Sub) {
            expr = mExprBuilder->Sub(intLHS,intRHS);
        } else if (binop.getOpcode() == Instruction::Mul) {
            expr = mExprBuilder->Mul(intLHS,intRHS);
        } else {
            assert(false && "Unsupported arithmetic instruction opcode");
        }

        mInstructions.push_back({*variable, expr});
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

void CfaBuilderVisitor::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    mInstructions.push_back({*selectVar, mExprBuilder->Select(cond, then, elze)});
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
        HANDLE_PREDICATE(CmpInst::ICMP_UGT, Gt)
        HANDLE_PREDICATE(CmpInst::ICMP_UGE, GtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_ULT, Lt)
        HANDLE_PREDICATE(CmpInst::ICMP_ULE, LtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SGT, Gt)
        HANDLE_PREDICATE(CmpInst::ICMP_SGE, GtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SLT, Lt)
        HANDLE_PREDICATE(CmpInst::ICMP_SLE, LtEq)
        default:
            assert(false && "Unhandled ICMP predicate.");
    }

    mInstructions.push_back({*icmpVar, expr});
}

void CfaBuilderVisitor::visitCastInst(llvm::CastInst& cast)
{
    auto variable = getVariable(&cast);
    auto castOp   = operand(cast.getOperand(0));

    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder->ZExt(castOp, variable->getType());
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder->SExt(castOp, variable->getType());
    } else {
        // Other cast types are ignored at the moment.
    }

    mInstructions.push_back({*variable, castOp});
}

void CfaBuilderVisitor::visitCallInst(llvm::CallInst& call)
{
    // Ignored.
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

static void printAutomaton(Automaton& cfa);

bool CfaBuilderPass::runOnFunction(Function& function)
{
    CfaBuilderVisitor visitor(function);

    std::unique_ptr<Automaton> cfa = visitor.transform();

    printAutomaton(*cfa);
    
    return false;
}


static void printAutomaton(Automaton& cfa)
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

