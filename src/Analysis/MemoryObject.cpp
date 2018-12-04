#include "gazer/Analysis/MemoryObject.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Analysis/AliasSetTracker.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace llvm;

void MemoryObject::print(llvm::raw_ostream& os) const
{
    static const char* const AllocTypeNames[] = {
        "Unknown",
        "Global",
        "Alloca",
        "FunctionCall"
    };

    static const char* const ObjectTypeNames[] = {
        "Unknown",
        "Scalar",
        "Array",
        "Struct"
    };

    os << "MemoryObject("
        << mID << ", "
        << (mName == "" ? "" : "\"" + mName + "\", ")
        << "alloc=" << AllocTypeNames[static_cast<unsigned>(mAllocType)]
        << ", size=" << mSize
        << ", objectType=" << ObjectTypeNames[static_cast<unsigned>(mObjectType)]
        << ", valueType=" << *mValueType << ")";
}

static llvm::Value* TrackPointerToOrigin(llvm::Value* ptr)
{
    llvm::errs() << "track " << *ptr << "\n";
    if (auto gep = dyn_cast<GEPOperator>(ptr)) {
        return TrackPointerToOrigin(gep->getPointerOperand());
    } else if (
        isa<AllocaInst>(ptr) || isa<CallInst>(ptr)
        || isa<GlobalValue>(ptr) || isa<PHINode>(ptr)
    ) {
        return ptr;
    } else {
        llvm::errs() << "Cannot track pointer origins " << *ptr << "\n";
        return nullptr;
    }
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const MemoryObject& memObject)
{
    memObject.print(os);
    return os;
}

auto MemoryObjectAnalysis::Create(
    llvm::Function& function, const llvm::DataLayout& dl,
    MemorySSA& memSSA, AAResults& aa
) -> std::unique_ptr<MemoryObjectAnalysis>
{
    unsigned memoryObjCnt = 0;
    llvm::DenseMap<llvm::Value*, MemoryObject*> allocations;
    std::vector<MemoryObject*> memoryObjects;

    // Add all global variables as memory objects.
    for (GlobalVariable& gv : function.getParent()->globals()) {
        MemoryObject* object = new MemoryObject(
            ++memoryObjCnt,
            MemoryAllocationType::Global,
            dl.getTypeAllocSize(gv.getValueType()),
            MemoryObjectType::Scalar,
            gv.getValueType(),
            gv.getName()
        );

        memoryObjects.push_back(object);
        allocations[&gv] = object;
    }

    // Add other possible memory objects
    for (Instruction& inst : llvm::instructions(function))
    {
        if (auto allocaInst = dyn_cast<AllocaInst>(&inst)) {
            MemoryObject* object = new MemoryObject(
                ++memoryObjCnt,
                MemoryAllocationType::Alloca,
                dl.getTypeAllocSize(allocaInst->getAllocatedType()),
                MemoryObjectType::Scalar,
                allocaInst->getAllocatedType()
            );
            memoryObjects.push_back(object);
            allocations[allocaInst] = object;
        } else if (auto call = dyn_cast<CallInst>(&inst)) {
            Function* callee = call->getCalledFunction();
            if (callee->getName() == "malloc") {
                // ...
            }
        }
    }

    memSSA.dump();

    auto walker = memSSA.getWalker();

    for (BasicBlock& bb : function) {
        for (Instruction& inst : bb) {
            if (auto store = dyn_cast<StoreInst>(&inst)) {
                auto memAccess = walker->getClobberingMemoryAccess(store);
                llvm::errs() << " Clobber: " << *memAccess << "\n";

                auto orig = TrackPointerToOrigin(store->getPointerOperand());
                if (orig != nullptr) {
                    llvm::errs() << " Object: " << *allocations[orig] << "\n";
                }
            } else if (auto load = dyn_cast<LoadInst>(&inst)) {
                auto memAccess = walker->getClobberingMemoryAccess(load);
                llvm::errs() << " Clobber: " << *memAccess << "\n";

                auto orig = TrackPointerToOrigin(load->getPointerOperand());
                if (orig != nullptr) {
                    llvm::errs() << " Object: " << *allocations[orig] << "\n";
                }
            }
        }
    }

    return nullptr;
}

void MemoryObjectAnalysis::getMemoryObjectsForPointer(llvm::Value* ptr, std::vector<MemoryObject*>& objects)
{
}

char MemoryObjectPass::ID;

void MemoryObjectPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<AAResultsWrapperPass>();
    au.addRequired<MemorySSAWrapperPass>();
}

bool MemoryObjectPass::runOnModule(llvm::Module& module)
{
    unsigned memoryObjCnt = 0;
    
    llvm::DenseMap<llvm::Value*, MemoryObject*> allocations;
    std::vector<MemoryObject*> memoryObjects;
    const llvm::DataLayout& dl = module.getDataLayout();

    // Add global variables as distinct memory objects
    for (GlobalVariable& gv : module.globals()) {
        MemoryObject* object = new MemoryObject(
            ++memoryObjCnt,
            MemoryAllocationType::Global,
            dl.getTypeAllocSize(gv.getValueType()),
            MemoryObjectType::Scalar,
            gv.getValueType(),
            gv.getName()
        );

        memoryObjects.push_back(object);
        allocations[&gv] = object;
    }

    for (Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        if (function.getName() != "main") {
            continue;
        }

        MemorySSA& memSSA = getAnalysis<MemorySSAWrapperPass>(function).getMSSA();
        AAResults& aa = getAnalysis<AAResultsWrapperPass>(function).getAAResults();

        auto result = MemoryObjectAnalysis::Create(function, dl, memSSA, aa);

    }

    return false;
}
