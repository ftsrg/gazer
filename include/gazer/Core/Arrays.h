#ifndef _GAZER_CORE_ARRAYS_H
#define _GAZER_CORE_ARRAYS_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/SmallVector.h>

#include <string>

namespace gazer
{

/**
 * Represents a single update in an array.
 */
class ArrayUpdate
{
public:
    ArrayUpdate(Variable* array, ArrayUpdate* previous)
        : mArray(array), mPrevious(previous), mSize(previous->getSize() + 1)
    {}

    ArrayUpdate(Variable* array)
        : mArray(array), mPrevious(nullptr), mSize(1)
    {}

    ArrayUpdate* getPrevious() const { return mPrevious; }
    Variable* getArrayVariable() const { return mArray; }
    unsigned int getSize() const { return mSize; }

    using child_iterator =
        llvm::SmallVector<std::unique_ptr<ArrayUpdate>, 1>::iterator;

    child_iterator child_begin() { return mChildren.begin(); }
    child_iterator child_end() { return mChildren.end(); }
    llvm::iterator_range<child_iterator> children() {
        return llvm::make_range(child_begin(), child_end());
    }

private:
    Variable* mArray;
    ArrayUpdate* mPrevious;
    llvm::SmallVector<std::unique_ptr<ArrayUpdate>, 1> mChildren;
    unsigned int mSize;
};

/**
 * This chain contains a tree of the updates of array
 */
class ArrayUpdateChain
{
    class ArrayWriteExpr;
public:
    ArrayUpdateChain(Variable* root, ArrayUpdate* head)
        : mRoot(root), mHead(head)
    {
        assert(mRoot->getType().isArrayType()
            && "Array updates can only work on arrays.");
    }

    ExprRef<ArrayWriteExpr> update(ExprPtr index, ExprPtr elem);

private:
    Variable* mRoot;
    std::unique_ptr<ArrayUpdate> mHead;
};
}

#endif
