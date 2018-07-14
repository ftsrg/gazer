#include "gazer/LLVM/Analysis/TopologicalSort.h"

#include <llvm/IR/CFG.h>
#include <llvm/ADT/PostOrderIterator.h>

#include <algorithm>

using namespace gazer;
using namespace llvm;

std::unique_ptr<TopologicalSort> TopologicalSort::Create(Function& function)
{
    std::vector<llvm::BasicBlock*> revSorted;
    po_iterator<BasicBlock*> begin = po_begin(&function.getEntryBlock());
    po_iterator<BasicBlock*> end   = po_end(&function.getEntryBlock());

    for (auto it = begin; it != end; ++it) {
        revSorted.push_back(*it);
    }

    std::reverse(revSorted.begin(), revSorted.end());

    return std::unique_ptr<TopologicalSort>(new TopologicalSort(revSorted));
}

char TopologicalSortPass::ID;

TopologicalSortPass::TopologicalSortPass()
    : FunctionPass(ID)
{}

void TopologicalSortPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.setPreservesAll();
}

bool TopologicalSortPass::runOnFunction(llvm::Function& function)
{
    mTopoSort = TopologicalSort::Create(function);

    return false;
}

llvm::Pass* gazer::createTopologicalSortPass() {
    return new TopologicalSortPass();
}

