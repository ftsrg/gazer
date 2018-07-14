#ifndef _GAZER_LLVM_ANALYSIS_TOPOLOGICALSORT_H
#define _GAZER_LLVM_ANALYSIS_TOPOLOGICALSORT_H

#include <llvm/IR/Function.h>
#include <llvm/Pass.h>

#include <vector>

namespace gazer
{

class TopologicalSort : private std::vector<llvm::BasicBlock*>
{
    TopologicalSort(std::vector<llvm::BasicBlock*>& blocks)
        : vector(blocks)
    {}
public:
    static std::unique_ptr<TopologicalSort> Create(llvm::Function& function);

public:
    using vector::iterator;
    using vector::const_iterator;
    using vector::reverse_iterator;
    using vector::const_reverse_iterator;

    using vector::begin;
    using vector::end;
    using vector::rbegin;
    using vector::rend;

    using vector::size;
    using vector::operator[];

    #if 0
    //--- Iterator access ---//
    using iterator = BlockVector::iterator;
    using const_iterator = BlockVector::const_iterator;
    using reverse_iterator = BlockVector::reverse_iterator;
    using const_reverse_iterator = BlockVector::const_reverse_iterator;

    iterator begin()  { return mSortedBlocks.begin(); }
    iterator end()    { return mSortedBlocks.end(); }
    reverse_iterator rbegin() { return mSortedBlocks.rbegin(); }
    reverse_iterator rend()   { return mSortedBlocks.rend(); }

    const_iterator begin() const { return mSortedBlocks.begin(); }
    const_iterator end() const { return mSortedBlocks.end(); }
    const_reverse_iterator rbegin() const { return mSortedBlocks.rbegin(); }
    const_reverse_iterator rend() const { return mSortedBlocks.rend(); }

    const BasicBlock* operator[](size_t idx) { return }

    #endif
};

class TopologicalSortPass final : public llvm::FunctionPass
{
public:
    static char ID;

    TopologicalSortPass();

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const;
    virtual bool runOnFunction(llvm::Function& function);

    TopologicalSort& getTopologicalSort() const { return *mTopoSort; }
private:
    std::unique_ptr<TopologicalSort> mTopoSort;
};

llvm::Pass* createTopologicalSortPass();

}

#endif
