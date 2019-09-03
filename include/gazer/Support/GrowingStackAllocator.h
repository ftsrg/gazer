#ifndef GAZER_SUPPORT_GROWINGSTACKALLOCATOR_H
#define GAZER_SUPPORT_GROWINGSTACKALLOCATOR_H

#include <llvm/Support/Allocator.h>

namespace gazer
{

/// An ever-growing stack-like allocator.
/// 
/// This allocator follows a stack-like allocation policy: any object may be allocated,
/// but deallocation can happen only at the end of the pool, i.e. the last object.
/// The pool behaves as an ever-growing storage, but is not a continuous chunk of memory.
template<class AllocatorT = llvm::MallocAllocator, size_t SlabSize = 4096>
class GrowingStackAllocator : public llvm::AllocatorBase<GrowingStackAllocator<AllocatorT, SlabSize>>
{
    struct Slab
    {
        void* Start;
        char* Current;
        char* End;

        void deallocate() { free(Start); }
    };
public:
    GrowingStackAllocator() = default;
    
    void Init()
    {
        this->startNewSlab();
    }

    LLVM_ATTRIBUTE_RETURNS_NONNULL void* Allocate(size_t size, size_t alignment)
    {
        size_t adjustment = llvm::alignmentAdjustment(slab().Current, alignment);
        assert(adjustment + size >= size);

        if (size > SlabSize) {
            llvm::report_bad_alloc_error("Requested allocation size is larger than GrowingStackAllocator slab size!");
        }

        if (slab().Current + adjustment + size > slab().End) {
            // If the requested size would overrun this slab, start a new one.
            this->startNewSlab();
        }

        auto alignedPtr = slab().Current + adjustment;
        slab().Current = alignedPtr + size;

        return alignedPtr;
    }

    void Deallocate(const void *ptr, size_t size)
    {
        // Stack allocators only allow deallocation on the end of the pool.
        auto ptrEnd = ((char*) ptr) + size;

        if (ptrEnd == slab().Current) {
            // The pointer is at the end of our current slab.
            slab().Current = (char*) ptr;
        } else {
            // The pointer must be at the end of the previous slab, get that now.
            Slab& prevSlab = mSlabs[mCurrentSlab - 1];
            assert(ptrEnd == prevSlab.Current);
            mCurrentSlab--;
            slab().Current = (char*) ptr;
        }
    }

    ~GrowingStackAllocator()
    {
        for (size_t i = 0; i < mSlabs.size(); ++i) {
            mSlabs[i].deallocate();
        }
    }

private:
    void startNewSlab()
    {
        if (mSlabs.empty() || mCurrentSlab == mSlabs.size() - 1) {
            void *newSlab = mAllocator.Allocate(SlabSize, 0);
            mSlabs.push_back({ newSlab, (char*) newSlab, (char*) newSlab + SlabSize });
            mCurrentSlab = mSlabs.size() - 1;
        } else {
            // There is an already allocated slab, we just need to change mCurrentSlab.
            mCurrentSlab++;
        }
    }

    Slab& slab() { return mSlabs[mCurrentSlab]; }

private:
    AllocatorT mAllocator;
    llvm::SmallVector<Slab, 32> mSlabs;
    size_t mCurrentSlab = 0;
};

}

#endif
