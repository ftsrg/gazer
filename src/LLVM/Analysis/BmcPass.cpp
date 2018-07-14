#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/BMC/BMC.h"
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Expr.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"

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

static ExprPtr encodeEdge(BasicBlock* from, BasicBlock* to, InstToExpr& ir2expr)
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

    // We are starting from the non-phi nodes at the 'from' part of the block
    for (auto it = from->getFirstInsertionPt(); it != from->end(); ++it) {
        exprs.push_back(ir2expr.transform(*it, succIdx));
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

    using ExprVector = std::vector<ExprPtr>;
    std::vector<ExprVector> dp(numBB, ExprVector(numBB, BoolLiteralExpr::getFalse()));

    llvm::SmallDenseMap<BasicBlock*, ExprPtr, 1> result;

    // The entry block is always reachable
    dp[0][0] = BoolLiteralExpr::getTrue();

    for (size_t j = 1; j < numBB; ++j) {
        //llvm::dbgs() << "Length i = " << i << "\n";
        for (size_t i = 1; i <= j; ++i) {
            BasicBlock* bb = topo[j];
            #if 0
            llvm::dbgs() << "Doing block ";
            bb->printAsOperand(llvm::dbgs());
            llvm::dbgs() << "\n";
            #endif
        
            llvm::SmallVector<ExprPtr, 2> exprs;

            for (BasicBlock* pred : llvm::predecessors(bb)) {
                auto predIt = blocks.find(pred);
                assert(predIt != blocks.end()
                    && "All blocks must be present in the block map");

                size_t predIdx = blocks[pred];
                assert(predIdx < j
                   && "Predecessors must be before block in a topological sort");
                
                //llvm::dbgs() << "Getting formula (" << (i - 1) << ", " << predIdx << ") ";
                ExprPtr predFormula = dp[i - 1][predIdx];            
                if (predFormula != BoolLiteralExpr::getFalse()) {
                    auto andFormula = builder->And(
                        predFormula,
                        encodeEdge(pred, bb, ir2expr)
                    );
                    exprs.push_back(andFormula);
                }
            }

            //llvm::errs() << "Doing i=" << i << " j=" << j << "\n";

            if (!exprs.empty()) {
                auto expr = builder->Or(exprs.begin(), exprs.end());

                if (errorBlocks.count(j) != 0) {
                    llvm::errs() << "Found error path to block '";
                    bb->printAsOperand(llvm::errs());
                    llvm::errs() << "' with the length of " << i << "\n";

                    Z3Solver solver;

                    llvm::errs() << "   Transforming formula.\n";

                    try {
                        solver.add(expr);
                        //expr->print(std::cerr);

                        llvm::errs() << "   Running solver.\n";
                        auto result = solver.run();

                        if (result == Solver::SAT) {
                            llvm::errs() << "   Formula is SAT\n";
                        } else if (result == Solver::UNSAT) {
                            llvm::errs() << "   Formula is UNSAT\n";
                            dp[i][j] = BoolLiteralExpr::getFalse();
                        } else {
                            llvm::errs() << "   Unknown solver state.";
                        }
                    } catch (z3::exception& e) {
                        llvm::errs() << e.msg() << "\n";
                    }
                } else {
                    dp[i][j] = expr;
                }
            }
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

        // We are looking for the largest index for which the formula is not false
        size_t length = numBB - 1;
        while (length > 0) {
            if (dp[length][idx] != BoolLiteralExpr::getFalse()) {
                break;
            }

            length--;
        }

        llvm::errs() << "Adding error block at (" << length << ", " << idx << ")\n";

        result[block] = dp[length][idx];
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
    auto formulae = encode(function, topo, st);

    #if 0
    for (auto& entry : formulae) {
        llvm::errs() << "Checking for error block '";
        entry.first->printAsOperand(llvm::errs());
        llvm::errs() << "'\n";

        Z3Solver solver;

        llvm::errs() << "   Transforming formula.\n";

        try {
            solver.add(entry.second);
            //entry.second->print(std::cerr);

            llvm::errs() << "   Running solver.\n";
            auto result = solver.run();

            if (result == Solver::SAT) {
                llvm::errs() << "   Formula is SAT\n";
            } else if (result == Solver::UNSAT) {
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
