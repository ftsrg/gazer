#include "gazer/Analysis/MemoryObject.h"

#include <llvm/Analysis/BasicAliasAnalysis.h>
#include <llvm/Analysis/MemorySSA.h>
#include <llvm/Analysis/AliasSetTracker.h>
#include <llvm/Analysis/GlobalsModRef.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Analysis/PtrUseVisitor.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/IteratedDominanceFrontier.h>

#include <queue>
#include <unordered_map>
#include <gazer/Analysis/MemoryObject.h>

using namespace gazer;
using namespace llvm;

namespace gazer
{

/// Visitor class used for identifying the uses of an allocation.
class MemoryPointerVisitor : public llvm::PtrUseVisitor<MemoryPointerVisitor>
{
    friend class PtrUseVisitor<MemoryPointerVisitor>;
    friend class InstVisitor<MemoryPointerVisitor>;
public:
    explicit MemoryPointerVisitor(const DataLayout& dl)
        : PtrUseVisitor(dl)
    {}

    void clear();
    void track(llvm::Value* ptr);

    const llvm::DenseSet<llvm::Value*>& getCurrentPtrSet() const { return mCurrentPtrSet; }
    const llvm::SmallDenseSet<llvm::Instruction*, 32>& getDefinitions() const { return mDefinitions; }
    const llvm::SmallDenseSet<llvm::Instruction*, 32>& getUses() const { return mUses; }

protected:
    void visitStoreInst(llvm::StoreInst& store);
    void visitLoadInst(llvm::LoadInst& load);
    void visitGetElementPtrInst(llvm::GetElementPtrInst& gep);
    void visitBitCastInst(llvm::BitCastInst& bc) ;
    void visitCallSite(llvm::CallSite cs);
    void visitInstruction(llvm::Instruction& inst);
    void visitPHINode(llvm::PHINode& phi);
    void visitSelectInst(llvm::SelectInst& select);

private:
    llvm::SmallDenseSet<llvm::Instruction*, 32> mDefinitions;
    llvm::SmallDenseSet<llvm::Instruction*, 32> mUses;

    llvm::DenseSet<llvm::Value*> mCurrentPtrSet;
};

/// Represents a memory accessing instruction
struct MemoryAccessInfo
{
    llvm::Instruction* Inst;
    llvm::SmallVector<MemoryObject*, 4> DefinedObjects;
    llvm::SmallVector<MemoryObject*, 4> UsedObjects;

    explicit MemoryAccessInfo(llvm::Instruction* inst)
        : Inst(inst)
    {}
};

struct MemoryUseDefInfo
{
    MemoryObject* TheObject;
    llvm::SmallVector<llvm::Instruction*, 32> Defs;
    llvm::SmallVector<llvm::Instruction*, 32> Uses;

    MemoryUseDefInfo(MemoryObject* object)
        : TheObject(object)
    {}
};

using AccessInfoMapT = std::unordered_map<llvm::Instruction*, MemoryAccessInfo>;

} // end namespace gazer

static MemoryObjectType GetMemoryObjectType(llvm::Type* type)
{
    if (type->isArrayTy()) {
        return MemoryObjectType::Array;
    } else if (type->isStructTy()) {
        return MemoryObjectType::Struct;
    } else {
        return MemoryObjectType::Scalar;
    }
}

static MemoryAccessInfo& GetOrInsertMemoryAccessInfo(llvm::Instruction* inst, AccessInfoMapT& map)
{
    auto it = map.find(inst);
    if (it != map.end()) {
        return it->second;
    }

    auto result = map.emplace(inst, inst);
    return (result.first)->second;
}

auto MemoryObjectAnalysis::Create(
    llvm::Function& function, const llvm::DataLayout& dl,
    llvm::DominatorTree& domTree, MemorySSA& memSSA, AAResults& aa
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
            GetMemoryObjectType(gv.getValueType()),
            gv.getValueType(),
            gv.getName()
        );

        memoryObjects.push_back(object);
        allocations[&gv] = object;
    }

    // Add other possible memory objects
    for (Instruction& inst : llvm::instructions(function)) {
        if (auto allocaInst = dyn_cast<AllocaInst>(&inst)) {
            Type* allocaTy = allocaInst->getAllocatedType();

            MemoryObject* object = new MemoryObject(
                ++memoryObjCnt,
                MemoryAllocationType::Alloca,
                dl.getTypeAllocSize(allocaInst->getAllocatedType()),
                GetMemoryObjectType(allocaTy),
                allocaInst->getAllocatedType()
            );
            memoryObjects.push_back(object);
            allocations[allocaInst] = object;
        }
    }

    //std::unordered_multimap<llvm::Instruction*, MemoryObjectDef*> defs;
    AccessInfoMapT accessInfo;
    llvm::ForwardIDFCalculator idf(domTree);
    MemoryPointerVisitor visitor(dl);

    for (auto entry : allocations) {
        llvm::Value* alloc = entry.first;
        MemoryObject* object = entry.second;

        MemoryUseDefInfo info(object);

        llvm::errs() << *object << "\n";
        visitor.track(alloc);

        llvm::errs() << "  alloc: " << *alloc << "\n";
        for (auto ptr : visitor.getCurrentPtrSet()) {
            llvm::errs() << "  ptr: " << *ptr << "\n";
        }
        for (auto def : visitor.getDefinitions()) {
            llvm::errs() << "  def: " << *def << "\n";
        }
        for (auto use : visitor.getUses()) {
            llvm::errs() << "  use: " << *use << "\n";
        }

        // Add all discovered definitions and uses.
        for (auto defInst : visitor.getDefinitions()) {
            MemoryAccessInfo& access = GetOrInsertMemoryAccessInfo(defInst, accessInfo);
            access.DefinedObjects.push_back(object);

            info.Defs.push_back(defInst);
        }
        for (auto useInst : visitor.getUses()) {
            MemoryAccessInfo& access = GetOrInsertMemoryAccessInfo(useInst, accessInfo);
            access.UsedObjects.push_back(object);

            info.Uses.push_back(useInst);
        }

        llvm::SmallPtrSet<BasicBlock*, 32> defBlocks;
        for (llvm::Instruction* def : info.Defs) {
            defBlocks.insert(def->getParent());
        }

        llvm::SmallPtrSet<BasicBlock*, 32> liveBlocks;
        for (llvm::Instruction* def : info.Defs) {
            defBlocks.insert(def->getParent());
        }

        // Calculate an SSA-form using iterated dominance frontiers.
        idf.setDefiningBlocks(defBlocks);
        idf.setLiveInBlocks(liveBlocks);

        llvm::SmallVector<BasicBlock*, 32> phiBlocks;
        idf.calculate(phiBlocks);
    }

    visitor.clear();

    return nullptr;
}

//------------------------- LLVM pass implementation ------------------------//

char MemoryObjectPass::ID;

void MemoryObjectPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<AAResultsWrapperPass>();
    au.addRequired<MemorySSAWrapperPass>();
    au.addRequired<DominatorTreeWrapperPass>();
}

bool MemoryObjectPass::runOnModule(llvm::Module& module)
{
    unsigned memoryObjCnt = 0;
    
    llvm::DenseMap<llvm::Value*, MemoryObject*> allocations;
    std::vector<MemoryObject*> memoryObjects;
    const llvm::DataLayout& dl = module.getDataLayout();

    for (Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        if (function.getName() != "main") {
            continue;
        }

        MemorySSA& memSSA = getAnalysis<MemorySSAWrapperPass>(function).getMSSA();
        AAResults& aa = getAnalysis<AAResultsWrapperPass>(function).getAAResults();
        DominatorTree& dt = getAnalysis<DominatorTreeWrapperPass>(function).getDomTree();

        auto result = MemoryObjectAnalysis::Create(function, dl, dt, memSSA, aa);
    }

    return false;
}


//-------------------------- Pointer use discovery --------------------------//

void MemoryPointerVisitor::track(llvm::Value* ptr)
{
    PtrUseVisitorBase::PtrInfo pi;
    if (auto inst = dyn_cast<Instruction>(ptr)) {
        this->visitPtr(*inst);
    } else if (auto gv = dyn_cast<GlobalValue>(ptr)) {
        for (auto user : gv->users()) {
            if (auto inst = dyn_cast<Instruction>(user)) {
                this->visitPtr(*inst);
            } else {
                llvm::errs() << "  Pointer has a non-instruction use!";
                llvm::errs() << "  " << *user << "\n";
            }
        }
    } else {
        llvm::errs() << "Untraceable pointer: " << *ptr << "\n";
    }

    //llvm_unreachable("Non-global and non-instruction memory object allocation?");
}

void MemoryPointerVisitor::clear()
{
    mCurrentPtrSet.clear();
    mDefinitions.clear();
    mUses.clear();
    PI.reset();
    Worklist.clear();
    VisitedUses.clear();
}

void MemoryPointerVisitor::visitStoreInst(llvm::StoreInst& store)
{
    mDefinitions.insert(&store);

    llvm::Value *ValOp = store.getValueOperand();

    if (ValOp == *U) {
        return PI.setEscapedAndAborted(&store);
    } else if (!IsOffsetKnown) {
        return PI.setAborted(&store);
    }
}

void MemoryPointerVisitor::visitLoadInst(llvm::LoadInst& load) {
    mUses.insert(&load);
}

void MemoryPointerVisitor::visitGetElementPtrInst(llvm::GetElementPtrInst& gep) {
    mCurrentPtrSet.insert(&gep);
    PtrUseVisitor::visitGetElementPtrInst(gep);
}

void MemoryPointerVisitor::visitBitCastInst(llvm::BitCastInst& bc) {
    PtrUseVisitor::visitBitCastInst(bc);
}

void MemoryPointerVisitor::visitCallSite(llvm::CallSite cs)
{
    mDefinitions.insert(cs.getInstruction());
    PtrUseVisitor::visitCallSite(cs);
}

void MemoryPointerVisitor::visitInstruction(llvm::Instruction& inst)
{
    mCurrentPtrSet.insert(&inst);
    PtrUseVisitor::visitInstruction(inst);
}

void MemoryPointerVisitor::visitPHINode(llvm::PHINode& phi)
{
    mCurrentPtrSet.insert(&phi);
    enqueueUsers(phi);
}

void MemoryPointerVisitor::visitSelectInst(llvm::SelectInst& select)
{
    mCurrentPtrSet.insert(&select);
    enqueueUsers(select);
}

//-------------------------------- Utilities --------------------------------//

void MemoryObject::print(llvm::raw_ostream& os) const
{
    static const char* const AllocTypeNames[] = {
        "Unknown",
        "Global",
        "Alloca",
        "Heap"
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

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const gazer::MemoryObject& memObject)
{
    memObject.print(os);
    return os;
}