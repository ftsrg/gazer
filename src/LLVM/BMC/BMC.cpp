#include "gazer/LLVM/BMC/BMC.h"
#include "gazer/Core/Utils/ExprUtils.h"
#include "gazer/Support/Stopwatch.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace llvm;

namespace
{
cl::opt<bool> DumpFormula(
    "dump-bmc-formula", cl::desc("Dump the encoded program formula to stderr."));

}

static bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer_error";
}

static bool isErrorBlock(const BasicBlock* bb)
{
    for (auto& instr : *bb) {
        if (instr.getOpcode() == llvm::Instruction::Call) {
            auto call = llvm::dyn_cast<llvm::CallInst>(&instr);
            llvm::Function* callee = call->getCalledFunction();

            if (isErrorFunctionName(callee->getName())) {
                return true;
            }
        }
    }

    return false;
}

BoundedModelChecker::BoundedModelChecker(
    llvm::Function& function,
    TopologicalSort& topo,
    ExprBuilder* exprBuilder,
    SolverFactory& solverFactory,
    llvm::raw_ostream& os
) :
    mFunction(function), mTopo(topo), mSolverFactory(solverFactory), mOS(os),
    mExprBuilder(exprBuilder),
    mIr2Expr(function, mSymbols, mExprBuilder, mVariables, &mVariableToValueMap)
{}

auto BoundedModelChecker::encode() -> ProgramEncodeMapT
{    
    size_t numBB = mFunction.getBasicBlockList().size();

    // Find blocks containing assertion failures
    llvm::SmallDenseMap<size_t, BasicBlock*, 1> errorBlocks;
    for (size_t i = 0; i < numBB; ++i) {
        if (isErrorBlock(mTopo[i])) {
            errorBlocks.insert({i, mTopo[i]});
        }
    }

    llvm::DenseMap<const BasicBlock*, size_t> blocks(numBB);
    for (size_t i = 0; i < numBB; ++i) {
        blocks.insert({mTopo[i], i});
    }

    // We are using a dynamic programming-based approach.
    // As the CFG is a DAG after unrolling, we can create a mTopoligcal sort
    // of its blocks. Then we create an array with the size of numBB, and
    // perform DP as the following:
    //  (0) dp[0] := True (as the entry node is always reachable)
    //  (1) dp[i] := Or(forall p in pred(i): And(dp[p], SMT(p,i)))
    // This way dp[err] will contain the SMT encoding of all bounded error paths.
    std::vector<ExprPtr> dp(numBB, BoolLiteralExpr::getFalse());
    ProgramEncodeMapT result;

    // The entry block is always reachable
    dp[0] = BoolLiteralExpr::getTrue();

    for (size_t i = 1; i < numBB; ++i) {
        BasicBlock* bb = mTopo[i];
        llvm::SmallVector<ExprPtr, 2> exprs;

        for (BasicBlock* pred : llvm::predecessors(bb)) {
            auto predIt = blocks.find(pred);
            assert(predIt != blocks.end()
                && "All blocks must be present in the block map");

            size_t predIdx = blocks[pred];
            assert(predIdx < i
                && "Predecessors must be before block in a mTopological sort");

            ExprPtr predFormula = dp[predIdx];
            if (predFormula != BoolLiteralExpr::getFalse()) {
                auto andFormula = mExprBuilder->And(
                    predFormula,
                    encodeEdge(pred, bb)
                );
                exprs.push_back(andFormula);
            }
        }

        if (!exprs.empty()) {
            auto expr = mExprBuilder->Or(exprs.begin(), exprs.end());
            dp[i] = expr;
        }
    }

    for (auto& entry : errorBlocks) {
        size_t idx = entry.first;
        BasicBlock* block  = entry.second;

        result[block] = dp[idx];
    }

    return result;
}

ExprPtr BoundedModelChecker::encodeEdge(BasicBlock* from, BasicBlock* to)
{
    ExprVector exprs;
    auto terminator = from->getTerminator();
    
    // Find out which branch we are taking
    size_t succIdx = 0;
    while (succIdx != terminator->getNumSuccessors()) {
        if (terminator->getSuccessor(succIdx) == to) {
            break;
        }

        succIdx++;
    }

    assert(succIdx != terminator->getNumSuccessors()
        && "'From' should be a parent of 'to'");

    auto cacheResult = mFormulaCache.find(from);
    if (cacheResult == mFormulaCache.end()) {
        std::vector<ExprPtr> fromExprs;

        // We are starting from the non-phi nodes at the 'from' part of the block
        for (auto it = from->getFirstInsertionPt(); it != from->end(); ++it) {
            if (it->isTerminator()) {
                // TODO: Quick hack to avoid handling terminators
                continue;
            }

            fromExprs.push_back(mIr2Expr.transform(*it));
        }

        ExprPtr fromExpr;
        if (fromExprs.size() >= 1) {
            fromExpr = mIr2Expr.getBuilder()->And(fromExprs.begin(), fromExprs.end());
        } else {
            fromExpr = mIr2Expr.getBuilder()->True();
        }            
        mFormulaCache[from] = fromExpr;
        exprs.push_back(fromExpr);
    } else {
        exprs.push_back(cacheResult->second);
    }

    // Handle the branch
    exprs.push_back(mIr2Expr.transform(*from->getTerminator(), succIdx));

    // Handle the PHI nodes
    for (auto it = to->begin(); it != to->getFirstInsertionPt(); ++it) {
        auto expr = mIr2Expr.transform(*it, succIdx, from);
        exprs.push_back(expr);
    }

    return mIr2Expr.getBuilder()->And(exprs.begin(), exprs.end());
}

BmcResult BoundedModelChecker::run()
{
    llvm::DenseMap<const Variable*, llvm::Value*> variableToValueMap;

    mOS << "Program size: \n";
    mOS << "   Blocks: " << mFunction.getBasicBlockList().size() << "\n";
    mOS << "Encoding program into SMT formula.\n";

    auto& context = mFunction.getContext();

    llvm::DenseMap<BasicBlock*, size_t> blocks(mTopo.size());
    llvm::DenseMap<BasicBlock*, llvm::Value*> preds;

    for (size_t i = 0; i < mTopo.size(); ++i) {
        blocks.insert({mTopo[i], i});
    }

    // Create predecessor identifications
    llvm::Type* predTy = llvm::IntegerType::get(context, 32);
    for (BasicBlock& bb : mFunction) {
        size_t bbID = blocks[&bb];

        std::string name = "pred" + std::to_string(bbID);
        auto phi = llvm::PHINode::Create(predTy, 0, name);

        for (BasicBlock* pred : llvm::predecessors(&bb)) {
            size_t predID = blocks[pred];
            phi->addIncoming(
                llvm::ConstantInt::get(
                    predTy,
                    llvm::APInt(predTy->getIntegerBitWidth(), predID)
                ),
                pred
            );
        }

        if (phi->getNumIncomingValues() > 1) {
            bb.getInstList().push_front(phi);
            preds[&bb] = phi;
            // Also insert this into the symbol table
            auto& variable = mSymbols.create(name, gazer::IntType::get(32));
            mVariables[phi] = &variable;
        } else {
            // Do not create a PHI node with only one argument
            if (phi->getNumIncomingValues() == 1) {
                preds[&bb] = phi->getIncomingValue(0);
            }

            phi->dropAllReferences();
            phi->deleteValue();
        }
    }

    ProgramEncodeMapT result = this->encode();
    for (auto& entry : result) {
        mOS << "Checking for error block '";
        entry.first->printAsOperand(mOS);
        mOS << "'\n";

        std::unique_ptr<Solver> solver = mSolverFactory.createSolver(mSymbols);

        mOS << "   Transforming formula.\n";

        try {
            //entry.second->print(mOS);
            if (DumpFormula) {
                FormatPrintExpr(entry.second, mOS);
                mOS << "\n";
            }
            solver->add(entry.second);

            mOS << "   Running solver.\n";
            auto status = solver->run();

            if (status == Solver::SAT) {
                mOS << "   Formula is SAT\n";
                Valuation model = solver->getModel();
                //model.print(mOS);
                
                // Display a counterexample trace
                auto trace = BmcTrace::Create(
                    mTopo,
                    blocks,
                    preds,
                    entry.first,
                    model,
                    mIr2Expr.getVariableMap()
                );

                auto writer = bmc::CreateTextTraceWriter(mOS);
                mOS << "Error trace:\n";
                mOS << "-----------\n";
                writer->write(*trace);

                mOS << "Assertion failure found.\n";
                return BmcResult::CreateUnsafe(std::move(trace));
            } else if (status == Solver::UNSAT) {
                mOS << "   Formula is UNSAT\n";
            } else {
                mOS << "   Unknown solver state.";
            }
        } catch (SolverError& e) {
            mOS << e.what() << "\n";
        }
    }

    return BmcResult::CreateSafe();
}
