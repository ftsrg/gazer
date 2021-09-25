//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Memory/MemoryInstructionHandler.h"
#include "gazer/LLVM/Memory/MemorySSA.h"

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Support/Warnings.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/CommandLine.h>

#define DEBUG_TYPE "FlatMemoryModel"

using namespace gazer;

// Flat memory model implementation
//==------------------------------------------------------------------------==//

namespace
{

const llvm::cl::opt<bool> FlatMemoryDumpMemSSA("flat-memory-dump-memssa");

class FlatMemoryModelInstTranslator;

struct CallInfo
{
    llvm::DenseMap<MemoryObject*, memory::CallDef*> defs;
    llvm::DenseMap<MemoryObject*, memory::CallUse*> uses;
};

struct FlatMemoryFunctionInfo
{
    MemoryObject* memory;
    MemoryObject* stackPointer;
    MemoryObject* framePointer;

    MemoryObjectUse* exitUse;

    // Maps lifted globals onto their corresponding memory objects.
    llvm::DenseMap<llvm::GlobalVariable*, MemoryObject*> globals;

    // Maps non-lifted globals to their addresses in memory.
    llvm::DenseMap<llvm::GlobalVariable*, ExprRef<LiteralExpr>> globalPointers;

    llvm::DenseMap<const llvm::CallBase*, CallInfo> calls;

    std::unique_ptr<memory::MemorySSA> memorySSA;
};

class FlatMemoryModel : public MemoryModel, public MemoryTypeTranslator
{
public:
    static constexpr unsigned GlobalBegin32 = 0x00000001;
    static constexpr unsigned StackBegin32  = 0x40000000;
    static constexpr unsigned HeapBegin32   = 0xF0000000;

    using DominatorTreeFuncTy = std::function<llvm::DominatorTree&(llvm::Function&)>;

public:
    FlatMemoryModel(
        GazerContext& context,
        const LLVMFrontendSettings& settings,
        llvm::Module& llvmModule,
        DominatorTreeFuncTy dominators
    );

    void insertCallDefsUses(
        llvm::CallBase* call, FlatMemoryFunctionInfo& info, memory::MemorySSABuilder& builder);

    MemoryTypeTranslator& getMemoryTypeTranslator() override { return *this; }
    
    MemoryInstructionHandler& getMemoryInstructionHandler(llvm::Function& function) override;

    gazer::Type& handlePointerType(const llvm::PointerType* type) override {
        return this->ptrType();
    }

    gazer::Type& handleArrayType(const llvm::ArrayType* type) override {
        return this->ptrType();
    }

public:
    gazer::BvType& ptrType() {
        return BvType::Get(mContext, mDataLayout.getPointerSizeInBits());
    }

    gazer::BvType& cellType() {
        return BvType::Get(mContext, 8);
    }

    gazer::ArrayType& memoryArrayType() {
        return ArrayType::Get(ptrType(), cellType());
    }

    ExprRef<BvLiteralExpr> ptrConstant(unsigned int addr) {
        return BvLiteralExpr::Get(ptrType(), addr);
    }

    const FlatMemoryFunctionInfo& getInfoFor(const llvm::Function* function) {
        assert(!function->isDeclaration());
        assert(mFunctions.count(function) != 0);

        return mFunctions[function];
    }

    const LLVMFrontendSettings& getSettings() const { return mSettings; }

private:
    const LLVMFrontendSettings& mSettings;
    const llvm::DataLayout& mDataLayout;
    std::unordered_map<const llvm::Function*, FlatMemoryFunctionInfo> mFunctions;
    std::unordered_map<
        const llvm::Function*, std::unique_ptr<MemoryInstructionHandler>> mTranslators;
    std::unique_ptr<ExprBuilder> mExprBuilder;
    LLVMTypeTranslator mTypes;
};

} // namespace

FlatMemoryModel::FlatMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    llvm::Module& llvmModule,
    DominatorTreeFuncTy dominators
) : MemoryTypeTranslator(context),
    mSettings(settings),
    mDataLayout(llvmModule.getDataLayout()),
    mTypes(*this, mSettings)
{
    // Initialize the expression builder
    mExprBuilder = CreateFoldingExprBuilder(mContext);

    // If the global variable never has its address taken, we can lift it from
    // the memory array into its own memory object, as distinct globals never alias.
    llvm::SmallPtrSet<llvm::GlobalVariable*, 8> liftedGlobals;
    llvm::SmallPtrSet<llvm::GlobalVariable*, 4> otherGlobals;

    for (llvm::GlobalVariable& gv : llvmModule.globals()) {
        // FIXME: We currently do not lift globals which have array or struct types.
        //if (!memory::isGlobalUsedAsPointer(gv) && gv.getType()->isSingleValueType()) {
        //    liftedGlobals.insert(&gv);
        //} else {
            otherGlobals.insert(&gv);
        //}
    }

    for (llvm::Function& function : llvmModule) {
        if (function.isDeclaration()) {
            continue;
        }

        bool isEntryFunction = mSettings.getEntryFunction(llvmModule) == &function;

        memory::MemorySSABuilder builder(function, mDataLayout, dominators(function));
        auto& info = mFunctions[&function];

        info.memory = builder.createMemoryObject(
            0, MemoryObjectType::Unknown, MemoryObject::UnknownSize, nullptr, "Memory");
        info.memory->setTypeHint(memoryArrayType());

        info.stackPointer = builder.createMemoryObject(
            1, MemoryObjectType::Unknown, mDataLayout.getPointerSize(), nullptr, "StackPtr");
        info.stackPointer->setTypeHint(ptrType());

        info.framePointer = builder.createMemoryObject(
            2, MemoryObjectType::Unknown, mDataLayout.getPointerSize(), nullptr, "FramePtr");
        info.framePointer->setTypeHint(ptrType());

        builder.createLiveOnEntryDef(info.memory);
        builder.createLiveOnEntryDef(info.stackPointer);
        builder.createLiveOnEntryDef(info.framePointer);

        // Handle global variables
        unsigned globalCnt = 2;
        info.globals.reserve(liftedGlobals.size());

        for (llvm::GlobalVariable* gv : liftedGlobals) {
            auto gvObj = builder.createMemoryObject(
                globalCnt++,
                MemoryObjectType::Scalar,
                mDataLayout.getTypeAllocSize(gv->getType()->getPointerElementType()),
                gv->getType()->getPointerElementType(),
                gv->getName()
            );
            info.globals[gv] = gvObj;

            if (isEntryFunction && gv->hasInitializer()) {
                builder.createGlobalInitializerDef(gvObj, gv);
            }
        }

        unsigned globalAddr = GlobalBegin32;
        info.globalPointers.reserve(otherGlobals.size());

        for (llvm::GlobalVariable* gv : otherGlobals) {
            unsigned siz = mDataLayout.getTypeAllocSize(gv->getType()->getPointerElementType());
            info.globalPointers[gv] = this->ptrConstant(globalAddr);
            globalAddr += siz;

            if (isEntryFunction && gv->hasInitializer()) {
                builder.createGlobalInitializerDef(info.memory, gv);
            }
        }

        // Handle definitions and uses in instructions.
        for (llvm::Instruction& inst : llvm::instructions(function)) {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
                builder.createStoreDef(info.memory, *store);
            } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
                builder.createLoadUse(info.memory, *load);
            } else if (auto call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                this->insertCallDefsUses(call, info, builder);
            } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
                assert(info.exitUse == nullptr && "There must be at most one return use!");
                info.exitUse = builder.createReturnUse(info.memory, *ret);
            } else if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
                builder.createAllocaDef(info.memory, *alloca);
                builder.createAllocaDef(info.stackPointer, *alloca);
            }
        }

        // All definitions and uses were added, build the memory SSA
        info.memorySSA = builder.build();

        if (FlatMemoryDumpMemSSA) {
            info.memorySSA->print(llvm::errs());
        }
    }
}

static bool isIntrinsic(llvm::Function* callee)
{
    llvm::StringRef name = callee->getName();
    return name.startswith("gazer.") || name.startswith("llvm.") || name.startswith("verifier.");
}

void FlatMemoryModel::insertCallDefsUses(
    llvm::CallBase* call, FlatMemoryFunctionInfo& info, memory::MemorySSABuilder& builder)
{
    llvm::Function* callee = call->getCalledFunction();

    if (callee == nullptr) {
        builder.createCallDef(info.memory, call);
        builder.createCallUse(info.memory, call);
        return;
    }

    if (isIntrinsic(callee)) {
        // TODO: This may need to change in the case of some intrinsics (e.g. llvm.memcpy)
        return;
    }

    if (callee->doesNotAccessMemory()) {
        return;
    }

    if (callee->isDeclaration()) {
        // TODO: We could handle some know function here or clobber the memory according to some configuration option.
        return;
    }

    bool definesMemory = !callee->doesNotReturn();

    auto& callInfo = info.calls[call];

    if (definesMemory) {
        callInfo.defs[info.memory] = builder.createCallDef(info.memory, call);
    }

    callInfo.uses[info.memory] = builder.createCallUse(info.memory, call);
    callInfo.uses[info.stackPointer] = builder.createCallUse(info.stackPointer, call);
    callInfo.uses[info.framePointer] = builder.createCallUse(info.framePointer, call);

    // FIXME: Add global clobbers
}

// Flat memory model instruction translation
//==------------------------------------------------------------------------==//

namespace
{

class FlatMemoryModelInstTranslator : public MemorySSABasedInstructionHandler
{
public:
    FlatMemoryModelInstTranslator(
        FlatMemoryModel& memoryModel, FlatMemoryFunctionInfo& info,
        ExprBuilder& builder, LLVMTypeTranslator& types, const llvm::DataLayout& dl
    ) : MemorySSABasedInstructionHandler(*info.memorySSA, types),
        mMemoryModel(memoryModel), mInfo(info), mExprBuilder(builder), mDataLayout(dl)
    {}

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep) override;

    ExprPtr handlePointerValue(const llvm::Value* value) override;

    ExprPtr handlePointerCast(
        const llvm::CastInst& cast,
        const ExprPtr& origPtr) override;

    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops) override;

    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda, llvm::ArrayRef<ExprRef<LiteralExpr>> elements) override;

    ExprPtr handleZeroInitializedAggregate(const llvm::ConstantAggregateZero* caz) override;

    void handleStore(
        const llvm::StoreInst& store,
        llvm2cfa::GenerationStepExtensionPoint& ep) override;

    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        llvm2cfa::GenerationStepExtensionPoint& ep) override;

    void handleCall(
        const llvm::CallBase* call,
        llvm2cfa::GenerationStepExtensionPoint& parentEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
        llvm::SmallVectorImpl<VariableAssignment>& outputAssignments) override;

    void handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override;

    ExprPtr isValidAccess(llvm::Value* ptr, const ExprPtr& expr) override;

private:
    ExprPtr handleGlobalInitializer(
        memory::GlobalInitializerDef* def,
        const ExprPtr& pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep);

    ExprPtr pointerOffset(const ExprPtr& pointer, unsigned offset) {
        return mExprBuilder.Add(pointer, BvLiteralExpr::Get(mMemoryModel.ptrType(), offset));
    }

    ExprPtr buildMemoryRead(
        gazer::Type& targetTy, unsigned size, const ExprPtr& array, const ExprPtr& pointer);
    
    ExprPtr buildMemoryWrite(
        const ExprPtr& array, const ExprPtr& value, const ExprPtr& pointer, unsigned size);

    BvType& bv8ty()
    {
        return BvType::Get(mMemoryModel.getContext(), 8);
    }

    memory::MemorySSA& getMemorySSA() const { return *mInfo.memorySSA; }

private:
    FlatMemoryModel& mMemoryModel;
    FlatMemoryFunctionInfo& mInfo;
    ExprBuilder& mExprBuilder;
    const llvm::DataLayout& mDataLayout;
};

} // namespace


auto FlatMemoryModelInstTranslator::handleAlloca(const llvm::AllocaInst& alloc, llvm2cfa::GenerationStepExtensionPoint& ep)
    -> ExprPtr
{
    MemoryObjectDef* spDef = mMemorySSA.getUniqueDefinitionFor(&alloc, mInfo.stackPointer);
    MemoryObjectDef* memDef = mMemorySSA.getUniqueDefinitionFor(&alloc, mInfo.memory);

    assert(memDef != nullptr && "There must be exactly one Memory definition for an alloca!");
    assert(spDef != nullptr && "There must be exactly one StackPtr definition for an alloca!");

    unsigned long size = mDataLayout.getTypeAllocSize(alloc.getAllocatedType());
    
    // This alloca returns the pointer to the current stack frame,
    // which is then advanced by the size of the allocated type.
    // We also clobber the relevant bytes of the memory array.
    ExprPtr ptr = ep.getAsOperand(spDef->getReachingDef());
    Variable* defVar = ep.getVariableFor(spDef);

    assert(defVar != nullptr && "The definition variable should have been inserted earlier!");

    ep.insertAssignment(defVar, mExprBuilder.Add(
        ptr, mMemoryModel.ptrConstant(size)
    ));

    ExprPtr resArray = ep.getAsOperand(memDef->getReachingDef());
    for (unsigned i = 0; i < size; ++i) {
        resArray = mExprBuilder.Write(
            resArray,
            this->pointerOffset(ptr, i),
            mExprBuilder.Undef(bv8ty())
        );
    }

    Variable* memVar = ep.getVariableFor(memDef);
    if (!ep.tryToEliminate(memDef, memVar, resArray)) {
        ep.insertAssignment(memVar, resArray);
    }

    return ptr;
}

auto FlatMemoryModelInstTranslator::handlePointerValue(const llvm::Value* value)
    -> ExprPtr
{
    if (auto gv = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
        if (auto globalPtr = mInfo.globalPointers.lookup(gv)) {
            return globalPtr;
        }
    }

    return UndefExpr::Get(mMemoryModel.ptrType());
}

ExprPtr FlatMemoryModelInstTranslator::handlePointerCast(
    const llvm::CastInst& cast,
    const ExprPtr& origPtr)
{
    switch (cast.getOpcode()) {
        case llvm::Instruction::BitCast:
            return origPtr;
        case llvm::Instruction::PtrToInt:
        case llvm::Instruction::IntToPtr: {
            Type& targetType = mTypes.get(cast.getDestTy());
            assert(targetType.isBvType() && "Can only do pointer casts to/from bit-vectors!");

            return mExprBuilder.BvResize(origPtr, llvm::cast<BvType>(targetType));
        }
        default:
            break;
    }

    llvm_unreachable("Unknown pointer cast instruction!");
}

ExprPtr FlatMemoryModelInstTranslator::handleGetElementPtr(
    const llvm::GetElementPtrInst& gep,
    llvm::ArrayRef<ExprPtr> ops)
{
    assert(ops.size() == gep.getNumOperands());
    assert(ops.size() >= 2);

    ExprPtr addr = ops[0];

    auto ti = llvm::gep_type_begin(gep);
    assert(std::distance(ti, llvm::gep_type_end(gep)) == ops.size() - 1);

    for (unsigned i = 1; i < ops.size(); ++i) {
        // Calculate the size of the current step
        unsigned long size = mDataLayout.getTypeAllocSize(ti.getIndexedType());

        // Index arguments may be integer types different from the pointer type.
        // Extend/truncate them into the proper pointer length.
        // As per the LLVM language reference:
        //  * When indexing into a (optionally packed) structure, only i32 integer constants are allowed.
        //  * Indexing into an array, pointer or vector, integers of any width are allowed, and they are not required to be constant.
        //  * These integers are treated as signed values where relevant.
        ExprPtr index = ops[i];
        auto& indexTy = llvm::cast<BvType>(index->getType());
        if (mMemoryModel.ptrType().getWidth() > indexTy.getWidth()) {
            // We use SExt here to preserve the sign in case of possibly negative indices.
            // This should not affect the struct member case, as we could only observe a difference if
            // the first bit of a i32 constant is 1, meaning that we would need to have at least
            // 2147483648 struct members for this behavior to occur.
            index = mExprBuilder.SExt(index, mMemoryModel.ptrType());
        } else if (mMemoryModel.ptrType().getWidth() < indexTy.getWidth()) {
            index = mExprBuilder.Trunc(index, mMemoryModel.ptrType());
        }

        addr = mExprBuilder.Add(
            addr, mExprBuilder.Mul(index, mMemoryModel.ptrConstant(size))
        );

        ++ti;
    }

    return addr;
}

ExprPtr FlatMemoryModelInstTranslator::handleConstantDataArray(
    const llvm::ConstantDataArray* cda, llvm::ArrayRef<ExprRef<LiteralExpr>> elements)
{
    assert(elements.size() == cda->getNumElements());

    ArrayLiteralExpr::Builder builder(ArrayType::Get(mMemoryModel.ptrType(), bv8ty()), BvLiteralExpr::Get(bv8ty(), 0));

    llvm::Type* elemTy = cda->getType()->getArrayElementType();
    unsigned long size = mDataLayout.getTypeAllocSize(elemTy);

    unsigned currentOffset = 0;
    for (unsigned i = 0; i < cda->getNumElements(); ++i) {
        ExprRef<LiteralExpr> lit = elements[i];

        if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(lit)) {
            if (bvLit->getType().getWidth() < 8) {
                builder.addValue(
                    mMemoryModel.ptrConstant(currentOffset),
                    mExprBuilder.BvLit(bvLit->getValue().zext(8)));
                currentOffset += 1;
            } else {
                for (unsigned j = 0; j < size; ++j) {
                    auto byteValue = mExprBuilder.BvLit(bvLit->getValue().extractBits(8, j * 8));

                    builder.addValue(
                        mMemoryModel.ptrConstant(currentOffset),
                        byteValue
                    );
                    currentOffset += j;
                }
            }
        } else if (auto boolLit = llvm::dyn_cast<BoolLiteralExpr>(lit)) {
            builder.addValue(
                mMemoryModel.ptrConstant(currentOffset),
                boolLit->getValue() ? mExprBuilder.BvLit8(1) : mExprBuilder.BvLit8(0)
            );
            currentOffset += 1;
        } else if (auto floatLit = llvm::dyn_cast<FloatLiteralExpr>(lit)) {
            llvm::APInt floatAsIeeeBv = floatLit->getValue().bitcastToAPInt();
            for (unsigned j = 0; j < size; ++j) {
                auto byteValue = mExprBuilder.BvLit(floatAsIeeeBv.extractBits(8, j * 8));
                builder.addValue(
                    mMemoryModel.ptrConstant(currentOffset),
                    byteValue
                );
                currentOffset += j;
            }
        } else {
            llvm_unreachable("Unsupported array type!");
        }
    }

    return builder.build();
}

ExprPtr FlatMemoryModelInstTranslator::handleZeroInitializedAggregate(const llvm::ConstantAggregateZero *caz)
{
    return ArrayLiteralExpr::GetEmpty(ArrayType::Get(mMemoryModel.ptrType(), bv8ty()), mExprBuilder.BvLit8(0));
}

void FlatMemoryModelInstTranslator::handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    // We only consider the entry block of 'main'
    if (mMemoryModel.getSettings().getEntryFunction(*bb.getModule()) != bb.getParent()) {
        return;
    }

    if (&bb != &bb.getParent()->getEntryBlock()) {
        return;
    }

    for (MemoryObjectDef& def : mMemorySSA.definitionAnnotationsFor(&bb)) {
        Variable* defVariable = ep.getVariableFor(&def);
        if (auto globalInit = llvm::dyn_cast<memory::GlobalInitializerDef>(&def)) {
            ExprPtr pointer = ep.getAsOperand(globalInit->getGlobalVariable());
            ExprPtr globalValue = this->handleGlobalInitializer(globalInit, pointer, ep);
            if (!ep.tryToEliminate(&def, defVariable, globalValue)) {
                ep.insertAssignment(defVariable, globalValue);
            }
        } else if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
            ExprPtr initVal;
            if (def.getObject() == mInfo.stackPointer || def.getObject() == mInfo.framePointer) {
                // TODO: 64 bit
                initVal = mMemoryModel.ptrConstant(FlatMemoryModel::StackBegin32);
            } else {
                initVal = mExprBuilder.Undef(defVariable->getType());
            }

            if (!ep.tryToEliminate(&def, defVariable, initVal)) {
                ep.insertAssignment(defVariable, initVal);
            }
        }
    }

}

void FlatMemoryModelInstTranslator::handleStore(
    const llvm::StoreInst& store,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    MemoryObjectDef* memoryDef = mMemorySSA.getUniqueDefinitionFor(&store, mInfo.memory);
    assert(memoryDef != nullptr && "There must be exactly one definition for Memory on a store!");

    auto size = mDataLayout.getTypeAllocSize(store.getValueOperand()->getType());

    ExprPtr array = ep.getAsOperand(memoryDef->getReachingDef());
    ExprPtr value = ep.getAsOperand(store.getValueOperand());
    ExprPtr pointer = ep.getAsOperand(store.getPointerOperand());

    Variable* defVariable = ep.getVariableFor(memoryDef);
    ExprPtr write = this->buildMemoryWrite(array, value, pointer, size);

    if (!ep.tryToEliminate(memoryDef, defVariable, write)) {
        ep.insertAssignment(defVariable, write);
    }
}

ExprPtr FlatMemoryModelInstTranslator::handleLoad(
    const llvm::LoadInst& load,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    MemoryObjectUse* use = mMemorySSA.getUniqueUseFor(&load, mInfo.memory);
    assert(use != nullptr && "Each load must have a valid use for Memory!");

    MemoryObjectDef* def = use->getReachingDef();

    Type& loadTy = mTypes.get(load.getType());
    ExprPtr array = ep.getAsOperand(def);

    unsigned size = mDataLayout.getTypeAllocSize(load.getType());
    assert(size >= 1);

    return this->buildMemoryRead(loadTy, size, array, ep.getAsOperand(load.getPointerOperand()));
}

void FlatMemoryModelInstTranslator::handleCall(
    const llvm::CallBase* call,
    llvm2cfa::GenerationStepExtensionPoint& parentEp,
    llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
    llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
    llvm::SmallVectorImpl<VariableAssignment>& outputAssignments)
{
    LLVM_DEBUG(llvm::dbgs() << "Handling call instruction " << *call << "\n");

    const llvm::Function* callee = call->getCalledFunction();
    assert(callee != nullptr);

    auto& calleeInfo = mMemoryModel.getInfoFor(callee);
    auto& callInstInfo = mInfo.calls[call];

    // Map the memory call definition to its return use.
    // We only define memory, as the stack pointer should be back to its
    // "original" position when the call returns.
    MemoryObjectUse* use = calleeInfo.memory->getExitUse();
    if (use != nullptr) {
        // It is possible that the return use is ommited if the function
        // does not return.
        outputAssignments.emplace_back(
            parentEp.getVariableFor(callInstInfo.defs[mInfo.memory]),
            calleeEp.getOutputVariableFor(use->getReachingDef())->getRefExpr()
        );
    }

    // Map the memory, stack pointer and frame pointer to the inputs.
    for (auto [actual, formal] : std::initializer_list<std::pair<MemoryObject*, MemoryObject*>>{ 
        { mInfo.memory, calleeInfo.memory },
        { mInfo.stackPointer, calleeInfo.stackPointer },
        { mInfo.framePointer, calleeInfo.framePointer }})
    {
        inputAssignments.emplace_back(
            calleeEp.getInputVariableFor(formal->getEntryDef()),
            parentEp.getAsOperand(callInstInfo.uses[actual]->getReachingDef())
        );
    }
}

ExprPtr FlatMemoryModelInstTranslator::isValidAccess(llvm::Value* ptr, const ExprPtr& expr)
{
    return mExprBuilder.True();
}

auto FlatMemoryModelInstTranslator::handleGlobalInitializer(
    memory::GlobalInitializerDef* def,
    const ExprPtr& pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep) -> ExprPtr
{        
    llvm::GlobalVariable* gv = def->getGlobalVariable();

    assert(def->getReachingDef() != nullptr
        && "GlobalInitializerDef's should have a reachable LiveOnEntry!");
    
    ExprPtr array = ep.getAsOperand(def->getReachingDef());
    unsigned size = mDataLayout.getTypeAllocSize(gv->getType()->getPointerElementType());

    if (!gv->hasInitializer()) {
        for (unsigned i = 0; i < size; ++i) {
            array = mExprBuilder.Write(
                array,
                this->pointerOffset(pointer, i),
                mExprBuilder.Undef(bv8ty())
            );
        }

        return array;
    }

    llvm::Value* initializer = gv->getInitializer();
    ExprPtr val = ep.getAsOperand(initializer);

    return this->buildMemoryWrite(array, val, pointer, size);
}

auto FlatMemoryModelInstTranslator::buildMemoryRead(
    gazer::Type& targetTy, unsigned size, const ExprPtr& array, const ExprPtr& pointer) -> ExprPtr
{
    assert(array != nullptr);
    assert(pointer != nullptr);
    assert(array->getType().isArrayType());

    switch (targetTy.getTypeID()) {
        case Type::BvTypeID: {
            ExprPtr result = mExprBuilder.Read(array, pointer);
            for (unsigned i = 1; i < size; ++i) {
                // TODO: Little/big endian
                result = mExprBuilder.BvConcat(
                    mExprBuilder.Read(array, this->pointerOffset(pointer, i)), result);
            }

            return result;
        }
        case Type::BoolTypeID: {
            ExprPtr result = mExprBuilder.NotEq(mExprBuilder.Read(array, pointer), mExprBuilder.BvLit8(0));
            for (unsigned i = 1; i < size; ++i) {
                result = mExprBuilder.And(
                    mExprBuilder.NotEq(
                        mExprBuilder.Read(array, this->pointerOffset(pointer, i)),
                        mExprBuilder.BvLit8(0)
                    ),
                    result
                );
            }
            return result;
        }
        case Type::FloatTypeID: {
            ExprPtr result = mExprBuilder.Read(array, pointer);
            for (unsigned i = 1; i < size; ++i) {
                // TODO: Little/big endian
                result = mExprBuilder.BvConcat(
                    mExprBuilder.Read(array, this->pointerOffset(pointer, i)), result);
            }

            return mExprBuilder.BvToFp(result, llvm::cast<FloatType>(targetTy));
        }
        default:
            // If it is not a convertible type, just undef it.
            emit_warning("Cannot represent result type %s of memory load", targetTy.getName().c_str());
            return mExprBuilder.Undef(targetTy);
    }

    llvm_unreachable("Unhandled target type!");
}

auto FlatMemoryModelInstTranslator::buildMemoryWrite(
    const ExprPtr& array, const ExprPtr& value, const ExprPtr& pointer, unsigned size) -> ExprPtr
{
    if (auto bvTy = llvm::dyn_cast<BvType>(&value->getType())) {
        if (bvTy->getWidth() == 8) {
            return mExprBuilder.Write(array, pointer, value);
        }

        if (bvTy->getWidth() < 8) {
            return mExprBuilder.Write(
                array,
                pointer,
                mExprBuilder.ZExt(value, bv8ty())
            );
        }

        // Otherwise, just build the contents byte-by-byte
        ExprPtr result = array;
        for (unsigned i = 0; i < size; ++i) {
            result = mExprBuilder.Write(
                result, this->pointerOffset(pointer, i), mExprBuilder.Extract(value, i * 8, 8)
            );
        }

        return result;
    }
    
    if (value->getType().isBoolType()) {
        return mExprBuilder.Write(
            array,
            pointer,
            mExprBuilder.Select(value, mExprBuilder.BvLit8(0x01), mExprBuilder.BvLit8(0x00))
        );
    }

    if (auto fltTy = llvm::dyn_cast<FloatType>(&value->getType())) {
        if (fltTy->getWidth() == 8) {
            return mExprBuilder.Write(array, pointer, mExprBuilder.FpToBv(value, bv8ty()));
        }

        assert(fltTy->getWidth() > 8);
        ExprPtr result = array;
        ExprPtr fpAsBv = mExprBuilder.FpToBv(value, BvType::Get(mMemoryModel.getContext(), fltTy->getWidth()));

        for (unsigned i = 0; i < size; ++i) {
            result = mExprBuilder.Write(
                result, this->pointerOffset(pointer, i), mExprBuilder.Extract(fpAsBv, i * 8, 8)
            );
        }

        return result;
    }

    LLVM_DEBUG(llvm::dbgs() << "[Missing feature] Could not represent write for type " << value->getType() << "\n");

    // The value is undefined - but even with unknown/unhandled types, we know
    // which bytes we want to modify -- we just do not know the value.
    ExprPtr result = array;
    for (unsigned i = 0; i < size; ++i) {
        result = mExprBuilder.Write(
            result,
            this->pointerOffset(pointer, i),
            mExprBuilder.Undef(bv8ty())
        );
    }

    return result;
}

auto FlatMemoryModel::getMemoryInstructionHandler(llvm::Function& function)
    -> MemoryInstructionHandler&
{
    auto it = mTranslators.find(&function);
    if (it == mTranslators.end()) {
        it = mTranslators.try_emplace(&function,
            std::make_unique<FlatMemoryModelInstTranslator>(
                *this, mFunctions[&function], *mExprBuilder, mTypes, mDataLayout
            )).first;
    }

    return *it->second;
}

// Factory function implementation
//==------------------------------------------------------------------------==//
auto gazer::CreateFlatMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    llvm::Module& llvmModule,
    std::function<llvm::DominatorTree&(llvm::Function&)> dominators
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<FlatMemoryModel>(context, settings, llvmModule, dominators);
}