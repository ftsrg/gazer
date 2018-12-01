/**
 * @file
 * This file contains the MemoryObject analysis classes.
 */
#ifndef _GAZER_ANALYSIS_MEMORYOBJECT_H
#define _GAZER_ANALYSIS_MEMORYOBJECT_H

#include <llvm/IR/Instruction.h>

namespace gazer
{

/// Represents the allocation type of a memory object.
enum class MemoryAllocationType : unsigned
{
    Unknown = 0,    ///< allocated in an unknown source
    Global,         ///< allocated as a global variable
    Alloca,         ///< allocated by an alloca instruction
    FunctionCall    ///< allocated by a function call (e.g. 'malloc')
};

/// A memory object is a continuous area of memory which does not
/// overlap with other memory objects.
class MemoryObject
{
public:
    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownSize = ~uint64_t(0);

public:
    MemoryAllocationType getAllocationType() const { return mAllocType; }
    MemoryObjectSize getSize() const { return mSize; }

private:
    MemoryObjectSize mSize;
    MemoryAllocationType mAllocType;
};


/// Represents a read or write acces to a memory object.
class MemoryObjectAccess
{
public:

    MemoryObject* getTarget() const { return mTarget; }
private:
    MemoryObject* mTarget;
};


///
class MemoryObjectAnalysis
{
public:
};

}

#endif
