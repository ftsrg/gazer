#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/BMC/BMC.h"
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Expr.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/Utils/ExprUtils.h"

#include "gazer/LLVM/Ir2Expr.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/CFG.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Debug.h>


#include <vector>

using namespace gazer;

using llvm::Function;
using llvm::BasicBlock;
using llvm::Instruction;

char BmcPass::ID = 0;

static bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer__error";
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
            fromExprs.push_back(ir2expr.transform(*it, succIdx));
        }

        auto fromExpr = ir2expr.getBuilder()->And(fromExprs.begin(), fromExprs.end());
        cache[from] = fromExpr;
        exprs.push_back(fromExpr);
    } else {
        exprs.push_back(cacheResult->second);
    }

    // Handle the PHI nodes and branches
    for (auto it = to->begin(); it != to->getFirstInsertionPt(); ++it) {
        exprs.push_back(ir2expr.transform(*it, succIdx));
    }

    return ir2expr.getBuilder()->And(exprs.begin(), exprs.end());
}

/**
 * Encodes the bounded reachability of assertion failures into SMT formulas.
 */
static llvm::SmallDenseMap<BasicBlock*, ExprPtr, 1>
    encode(Function& function, TopologicalSort& topo, SymbolTable& symbols)
{
    auto builder = CreateFoldingExprBuilder();
    //auto builder = CreateExprBuilder();
    InstToExpr ir2expr(function, symbols, builder.get());

    size_t numBB = function.getBasicBlockList().size();

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

    llvm::DenseMap<const BasicBlock*, ExprPtr> formulaCache;

    std::vector<ExprPtr> dp(numBB, BoolLiteralExpr::getFalse());
    llvm::SmallDenseMap<BasicBlock*, ExprPtr, 1> result;

    // The entry block is always reachable
    dp[0] = BoolLiteralExpr::getTrue();

    for (size_t i = 1; i < numBB; ++i) {
        BasicBlock* bb = topo[i];       
        llvm::SmallVector<ExprPtr, 2> exprs;

        for (BasicBlock* pred : llvm::predecessors(bb)) {
            auto predIt = blocks.find(pred);
            assert(predIt != blocks.end() && "All blocks must be present in the block map");

            size_t predIdx = blocks[pred];
            assert(predIdx < i && "Predecessors must be before block in a topological sort");

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
            // Try to optimize for the binary case
            if (exprs.size() == 2) {
                // The folding builder may have optimized the And operands,
                // therefore we need to check
                if (exprs[0]->getKind() == Expr::And && exprs[1]->getKind() == Expr::And) {
                    // (F1 & F2) | (F1 & F3) => F1 & (F1 | F3)
                }
            }

            auto expr = builder->Or(exprs.begin(), exprs.end());
            dp[i] = expr;
        }
    }

    #if 0
    for (size_t i = 0; i < numBB; ++i) {
        llvm::errs() << i << " : ";
        topo[i]->printAsOperand(llvm::errs());
        llvm::errs() << "\n";
    }
    #endif

    #if 0
    for (size_t i = 0; i < numBB; ++i) {
        for (size_t j = 0; j < numBB; ++j) {
            std::cerr << "dp[" << i << ", " << j << "] = ";
            dp[i][j]->print(std::cerr);
            std::cerr << "\n";
        }
    }

    #endif

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
    //au.addRequired<CfaBuilderPass>();

    au.setPreservesAll();
}

bool BmcPass::runOnFunction(llvm::Function& function)
{
    TopologicalSort& topo = getAnalysis<TopologicalSortPass>().getTopologicalSort();    
    //Automaton& cfa = getAnalysis<CfaBuilderPass>().getCFA();

    SymbolTable st;

    llvm::errs() << "Program size: \n";
    llvm::errs() << "   Blocks: " << function.getBasicBlockList().size() << "\n";
    llvm::errs() << "Encoding program into SMT formula.\n";
    auto result = encode(function, topo, st);

    #if 1
    for (auto& entry : result) {
        llvm::errs() << "Checking for error block '";
        entry.first->printAsOperand(llvm::errs());
        llvm::errs() << "'\n";

        CachingZ3Solver solver;

        llvm::errs() << "   Transforming formula.\n";

        try {
            //entry.second->print(llvm::errs());
            //FormatPrintExpr(entry.second, llvm::errs());
            //llvm::errs() << "\n";
            solver.add(entry.second);

            llvm::errs() << "   Running solver.\n";
            auto status = solver.run();

            if (status == Solver::SAT) {
                llvm::errs() << "   Formula is SAT\n";
                solver.getModel();
            } else if (status == Solver::UNSAT) {
                llvm::errs() << "   Formula is UNSAT\n";
            } else {
                llvm::errs() << "   Unknown solver state.";
            }
        } catch (z3::exception& e) {
            llvm::errs() << e.msg() << "\n";
        }
    }
    #endif

    return false;
}
