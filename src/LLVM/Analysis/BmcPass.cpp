#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Expr.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/Utils/ExprUtils.h"

#include "gazer/LLVM/BMC/BMC.h"

#include "gazer/LLVM/Ir2Expr.h"
#include "gazer/Support/Stopwatch.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/CFG.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Debug.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InstIterator.h>

#include <vector>

using namespace gazer;
using namespace llvm;

char BmcPass::ID = 0;

static bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer_error";
}

static bool isErrorBlock(const BasicBlock& bb)
{
    for (auto& instr : bb) {
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

static ExprPtr encodeEdge(
    BasicBlock* from,
    BasicBlock* to,
    InstToExpr& ir2expr,
    llvm::DenseMap<const BasicBlock*, ExprPtr>& cache)
{
    /*
    llvm::errs() << "encoding edge between ";
    from->printAsOperand(llvm::errs());
    llvm::errs() << " and ";
    to->printAsOperand(llvm::errs());
    llvm::errs() << "\n";
    */

    std::vector<ExprPtr> exprs;

    // Find which branch we are taking
    auto terminator = from->getTerminator();

    size_t succIdx = 0;
    while (succIdx != terminator->getNumSuccessors()) {
        if (terminator->getSuccessor(succIdx) == to) {
            break;
        }

        succIdx++;
    }

    assert(succIdx != terminator->getNumSuccessors()
        && "'From' should be a parent of 'to'");

    auto cacheResult = cache.find(from);
    if (cacheResult == cache.end()) {
        std::vector<ExprPtr> fromExprs;

        // We are starting from the non-phi nodes at the 'from' part of the block
        for (auto it = from->getFirstInsertionPt(); it != from->end(); ++it) {
            if (it->isTerminator()) {
                // TODO: Quick hack to avoid handling terminators
                continue;
            }

            fromExprs.push_back(ir2expr.transform(*it));
        }

        auto fromExpr = ir2expr.getBuilder()->And(fromExprs.begin(), fromExprs.end());
        cache[from] = fromExpr;
        exprs.push_back(fromExpr);
    } else {
        exprs.push_back(cacheResult->second);
    }

    // Handle the branch
    exprs.push_back(ir2expr.transform(*from->getTerminator(), succIdx));

    // Handle the PHI nodes
    for (auto it = to->begin(); it != to->getFirstInsertionPt(); ++it) {
        auto expr = ir2expr.transform(*it, succIdx, from);
        //FormatPrintExpr(expr, llvm::errs());
        //gllvm::errs() << "\n";
        exprs.push_back(expr);
    }

    return ir2expr.getBuilder()->And(exprs.begin(), exprs.end());
}

/**
 * Encodes the bounded reachability of assertion failures into SMT formulas.
 */
static llvm::SmallDenseMap<BasicBlock*, ExprPtr, 1> encode(
    Function& function,
    TopologicalSort& topo,
    SymbolTable& symbols,
    InstToExpr& ir2expr
) {
    ExprBuilder* builder = ir2expr.getBuilder();
    llvm::DenseMap<const BasicBlock*, ExprPtr> formulaCache;

    size_t numBB = function.getBasicBlockList().size();

    // Find blocks containing assertion failures
    llvm::SmallDenseMap<size_t, BasicBlock*, 1> errorBlocks;
    for (size_t i = 0; i < numBB; ++i) {
        if (isErrorBlock(*topo[i])) {
            errorBlocks.insert({i, topo[i]});
        }
    }

    llvm::DenseMap<const BasicBlock*, size_t> blocks(numBB);
    for (size_t i = 0; i < numBB; ++i) {
        blocks.insert({topo[i], i});
    }

    // We are using a dynamic programming-based approach.
    // As the CFG is a DAG after unrolling, we can create a topoligcal sort
    // of its blocks. Then we create an array with the size of numBB, and
    // perform DP as the following:
    //  (0) dp[0] := True (as the entry node is always reachable)
    //  (1) dp[i] := Or(forall p in pred(i): And(dp[p], SMT(p,i)))
    // This way dp[err] will contain the SMT encoding of all bounded error paths.
    std::vector<ExprPtr> dp(numBB, BoolLiteralExpr::getFalse());
    llvm::SmallDenseMap<BasicBlock*, ExprPtr, 1> result;

    // The entry block is always reachable
    dp[0] = BoolLiteralExpr::getTrue();

    for (size_t i = 1; i < numBB; ++i) {
        BasicBlock* bb = topo[i];
        llvm::SmallVector<ExprPtr, 2> exprs;

        for (BasicBlock* pred : llvm::predecessors(bb)) {
            auto predIt = blocks.find(pred);
            assert(predIt != blocks.end()
                && "All blocks must be present in the block map");

            size_t predIdx = blocks[pred];
            assert(predIdx < i
                && "Predecessors must be before block in a topological sort");

            ExprPtr predFormula = dp[predIdx];
            if (predFormula != BoolLiteralExpr::getFalse()) {
                auto andFormula = builder->And(
                    predFormula,
                    encodeEdge(pred, bb, ir2expr, formulaCache)
                );
                exprs.push_back(andFormula);
            }
        }

        if (!exprs.empty()) {
            auto expr = builder->Or(exprs.begin(), exprs.end());
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

void BmcPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<TopologicalSortPass>();
    au.setPreservesCFG();
}

bool BmcPass::runOnFunction(llvm::Function& function)
{
    TopologicalSort& topo = getAnalysis<TopologicalSortPass>()
        .getTopologicalSort();    

    Stopwatch<> sw;
    Z3SolverFactory solverFactory;
    auto builder = CreateFoldingExprBuilder();

    sw.start();
    
    BoundedModelChecker bmc(function, topo, builder.get(), solverFactory, llvm::errs());
    bmc.run();

    sw.stop();
    llvm::errs() << "Elapsed time: ";
    sw.format(llvm::errs(), "s");
    llvm::errs() << "\n";

    // We modified the CFG with the predecessors identifications
    return true;
}
