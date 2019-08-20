#ifndef GAZER_LLVM_MEMORY_MEMORYOBJECT_H
#define GAZER_LLVM_MEMORY_MEMORYOBJECT_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/Type.h>

namespace gazer
{

namespace memory
{

class ObjectType
{
public:

private:

};

class StructType
{
public:

private:
    std::vector<Field> mFields;
    llvm::DenseMap<uint64_t, Field> mOffsets;
};

class Field
{
public:
    Field(uint64_t idx, uint64_t offset, llvm::Type* elemTy)
        : mIdx(idx), mOffset(offset), mElemType(elemTy)
    {}

private:
    uint64_t mIdx;
    uint64_t mOffset;
    llvm::Type* mElemType;
};

} // end namespace gazer::memory

} // end namespace gazer

#endif
