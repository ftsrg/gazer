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
//
/// \file This memory model declares a single flat memory object that represents
/// the entire memory. Globals which do not have their address taken are put
/// into their own partitions for efficiency.
//
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/Memory/MemoryUtils.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Support/Math.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/GlobalStatus.h>

using namespace gazer;

namespace
{

class FlatMemoryModel : public MemoryModel
{
    struct MemoryObjectDefInfo
    {
        MemoryObjectDef* Def;
        Variable* AllocVar;
        Variable* SizeVar;
    };

    struct CallInfo
    {
        llvm::DenseMap<MemoryObject*, memory::CallUse*> Uses;
        llvm::DenseMap<MemoryObject*, memory::CallDef*> Defs;
    };

    struct FunctionInfo
    {
        MemoryObject* Memory;
        MemoryObject* StackPointer;
        MemoryObject* FramePointer;

        llvm::DenseMap<MemoryObject*, memory::LiveOnEntryDef*> EntryDefs;
        llvm::DenseMap<MemoryObject*, memory::RetUse*> ExitUses;

        llvm::DenseMap<const llvm::GlobalVariable*, ExprPtr> GlobalPointers;
        llvm::DenseMap<const llvm::Value*, MemoryObject*> Globals;
        llvm::DenseMap<const llvm::Value*, CallInfo> Calls;
    };

    // Memory object pointers are disambiguated using the highest bits of the pointer:
    //  00... -> Stack
    //  01... -> Global
    //  1.... -> Heap
    static constexpr unsigned StackBegin  = 0x00000000;
    static constexpr unsigned GlobalBegin = 0x40000000;
    static constexpr unsigned HeapBegin   = 0x80000000;

public:
    FlatMemoryModel(
        GazerContext& context, LLVMFrontendSettings settings, const llvm::DataLayout& dl
    ) : MemoryModel(context, settings, dl)
    {
        mExprBuilder = mSettings.simplifyExpr
                        ? CreateFoldingExprBuilder(context)
                        : CreateExprBuilder(context);
    }

public:
    void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint) override {}
    
    ExprPtr handleLiveOnEntry(
        memory::LiveOnEntryDef* def,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    ExprPtr handlePointerCast(const llvm::CastInst& cast, ExprPtr opPtr) override
    {
        return opPtr;
    }

    ExprPtr handlePointerValue(const llvm::Value* value, llvm::Function& parent) override
    {
        auto& info = mFunctionInfo[&parent];

        if (auto gv = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
            auto globalPtr = info.GlobalPointers.lookup(gv);
            if (globalPtr != nullptr) {
                return globalPtr;
            }
        }

        return UndefExpr::Get(this->getPointerType());
    }

    /// Translates the given LoadInst into an assignable expression.
    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    void handleStore(
        const llvm::StoreInst& store,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    /// Maps the given memory object to a memory object in function.
    void handleCall(
        llvm::ImmutableCallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        std::vector<VariableAssignment>& inputAssignments,
        std::vector<VariableAssignment>& outputAssignments,
        std::vector<VariableAssignment>& additionalAssignments
    ) override;

    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops
    ) override;

    void handleBlock(
        const llvm::BasicBlock& bb,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    gazer::Type& handlePointerType(const llvm::PointerType* type) override {
        return this->getPointerType();
    }

    /// Translates type for constant arrays and initializers.
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override {
        return this->getPointerType();
    }

    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda,
        llvm::ArrayRef<ExprRef<LiteralExpr>> elements
    ) override {
        assert(elements.size() == cda->getNumElements());

        ArrayLiteralExpr::Builder builder(ArrayType::Get(
            this->getPointerType(),
            BvType::Get(mContext, 8)
        ));

        llvm::Type* elemTy = cda->getType()->getArrayElementType();
        unsigned size = mDataLayout.getTypeAllocSize(elemTy);

        unsigned currentOffset = 0;
        for (unsigned i = 0; i < cda->getNumElements(); ++i) {
            ExprRef<LiteralExpr> lit = elements[i];

            if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(lit)) {
                if (bvLit->getType().getWidth() < 8) {
                    builder.addValue(
                        this->ptrConstant(currentOffset),
                        mExprBuilder->BvLit(bvLit->getValue().zext(8))
                    );
                    currentOffset += 1;
                } else {
                    for (unsigned j = 0; j < size; ++j) {
                        auto byteValue = mExprBuilder->BvLit(bvLit->getValue().extractBits(8, j * 8));

                        builder.addValue(
                            this->ptrConstant(currentOffset),
                            byteValue
                        );
                        currentOffset += j;
                    }
                }
            } else if (auto boolLit = llvm::dyn_cast<BoolLiteralExpr>(lit)) {
                builder.addValue(
                    this->ptrConstant(currentOffset),
                    boolLit->getValue() ? mExprBuilder->BvLit8(1) : mExprBuilder->BvLit8(0)
                );
                currentOffset += 1;
            } else {
                llvm_unreachable("Unsupported array type!");
            }
        }

        return builder.build();
    }


    ExprPtr handleGlobalInitializer(
        memory::GlobalInitializerDef* def,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override
    {
        llvm::GlobalVariable* gv = def->getGlobalVariable();
        llvm::Value* initializer = gv->getInitializer();

        assert(def->getReachingDef() != nullptr);

        auto& info = mFunctionInfo[def->getParentBlock()->getParent()];
        if (auto gvObj = info.Globals.lookup(gv)) {
            if (gv->hasInitializer()) {
                return ep.getAsOperand(initializer);
            }

            return mExprBuilder->Undef(def->getObject()->getObjectType());
        }

        ExprPtr array = ep.getAsOperand(def->getReachingDef());
        unsigned size = mDataLayout.getTypeAllocSize(initializer->getType());

        if (!gv->hasInitializer()) {
            for (unsigned i = 0; i < size; ++i) {
                array = mExprBuilder->Write(
                    array,
                    this->pointerOffset(pointer, i),
                    mExprBuilder->Undef(BvType::Get(mContext, 8))
                );
            }

            return array;
        }

        ExprPtr val = ep.getAsOperand(initializer);

        if (val->getType().isBvType()) {
            for (unsigned i = 0; i < size; ++i) {
                array = mExprBuilder->Write(
                    array,
                    this->pointerOffset(pointer, i),
                    mExprBuilder->Extract(val, i * 8, 8)
                );
            }
        } else if (val->getType().isBoolType()) {
            array = mExprBuilder->Write(
                array,
                pointer,
                mExprBuilder->Select(val, mExprBuilder->BvLit8(0x01), mExprBuilder->BvLit8(0x00))
            );
        } else {
            // Even with unknown/unhandled types, we know which bytes we modify -- we just
            // do not know the value.
            for (unsigned i = 0; i < size; ++i) {
                array = mExprBuilder->Write(
                    array,
                    this->pointerOffset(pointer, i),
                    mExprBuilder->Undef(this->getMemoryCellType())
                );
            }
        }

        return array;
    }

protected:
    void initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder) override;

private:
    BvType& getPointerType() const { return BvType::Get(mContext, mDataLayout.getPointerSizeInBits()); }
    ExprRef<BvLiteralExpr> ptrConstant(unsigned addr) {
        return BvLiteralExpr::Get(getPointerType(), addr);
    }
    
    ArrayType& getMemoryObjectType() const {
        return ArrayType::Get(this->getPointerType(), this->getMemoryCellType());
    }

    ExprPtr pointerOffset(ExprPtr pointer, unsigned offset) {
        return mExprBuilder->Add(pointer, BvLiteralExpr::Get(getPointerType(), offset));
    }

    Type& getMemoryCellType() const
    {
        if (mSettings.ints == IntRepresentation::Integers) {
            return IntType::Get(mContext);
        }

        return BvType::Get(mContext, 8);
    }

    ExprPtr buildMemoryRead(Type& targetTy, unsigned size, ExprPtr array, ExprPtr pointer);

private:
    llvm::DenseMap<const llvm::Function*, FunctionInfo> mFunctionInfo;
    std::unique_ptr<ExprBuilder> mExprBuilder;
};

} // end anonymous namespace

static bool hasPointerOperands(llvm::Function& func)
{
    return std::any_of(func.arg_begin(), func.arg_end(), [](llvm::Argument& arg) {
        return arg.getType()->isPointerTy();
    });
}

static bool hasUsesInFunction(llvm::GlobalVariable& gv, llvm::Function& func)
{
    return std::any_of(gv.user_begin(), gv.user_end(), [&func](llvm::User* user) {
        return llvm::isa<llvm::Instruction>(user)
            && llvm::cast<llvm::Instruction>(user)->getFunction() == &func;
    });
}

void FlatMemoryModel::initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder)
{
    auto& info = mFunctionInfo[&function];
    // TODO: This should be more flexible.
    bool isEntryFunction = function.getName() == "main";

    info.Memory = builder.createMemoryObject(
        0,
        this->getMemoryObjectType(),
        MemoryObject::UnknownSize,
        llvm::Type::getInt8Ty(function.getContext()),
        "Memory"
    );
    info.StackPointer = builder.createMemoryObject(
        1,
        this->getPointerType(),
        MemoryObject::UnknownSize,
        nullptr,
        "StackPointer"
    );
    info.FramePointer = builder.createMemoryObject(
        2,
        this->getPointerType(),
        MemoryObject::UnknownSize,
        nullptr,
        "FramePointer"
    );

    info.EntryDefs[info.Memory] = builder.createLiveOnEntryDef(info.Memory);
    info.EntryDefs[info.StackPointer] = builder.createLiveOnEntryDef(info.StackPointer);
    info.EntryDefs[info.FramePointer] = builder.createLiveOnEntryDef(info.FramePointer);

    // Handle global variables
    unsigned globalAddr = GlobalBegin;

    unsigned globalCnt = 2;
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        // If the global variable never has its address taken, we can lift it from the memory array
        // into its own memory object, as distinct globals never alias.
        // FIXME: Analyzing globals should be done once per module, not once per function.
        bool hasAddressTaken = memory::isGlobalUsedAsPointer(gv);

        unsigned siz = mDataLayout.getTypeAllocSize(gv.getType()->getPointerElementType());

        MemoryObject* gvObject;
        if (!hasAddressTaken) {
            gvObject = builder.createMemoryObject(
                ++globalCnt,
                translateType(gv.getType()->getPointerElementType()),
                siz,
                gv.getType()->getPointerElementType(),
                gv.getName()
            );
            info.Globals[&gv] = gvObject;
            info.EntryDefs[gvObject] = builder.createLiveOnEntryDef(gvObject);
        } else {
            gvObject = info.Memory;
            info.GlobalPointers[&gv] = this->ptrConstant(globalAddr);
            globalAddr += siz;
        }

        if (isEntryFunction && gv.hasInitializer()) {
            builder.createGlobalInitializerDef(gvObject, &gv);
        }
    }

    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            if (auto gvObj = info.Globals.lookup(store->getPointerOperand())) {
                builder.createStoreDef(gvObj, *store);
            } else {
                builder.createStoreDef(info.Memory, *store);
            }
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            if (auto gvObj = info.Globals.lookup(load->getPointerOperand())) {
                builder.createLoadUse(gvObj, *load);
            } else {
                builder.createLoadUse(info.Memory, *load);
            }
        } else if (auto call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
            llvm::Function* callee = call->getCalledFunction();

            if (callee == nullptr) {
                // TODO: Indirect calls.
                builder.createCallDef(info.Memory, call);
                builder.createCallUse(info.Memory, call);
                continue;
            }

            if (callee->getName().startswith("gazer.")
                || callee->getName().startswith("llvm.")
                || callee->getName().startswith("verifier.")
            ) {
                // TODO: This may need to change in the case of some intrinsics (e.g. llvm.memcpy)
                continue;
            }

            // Currently we assume that function declarations which return a value do not modify
            // global variables.
            // FIXME: We should make this configurable.
            if (!callee->isDeclaration() || callee->getReturnType()->isVoidTy()) {
                auto& callInfo = info.Calls[call];

                callInfo.Defs[info.Memory] = builder.createCallDef(info.Memory, call);
                callInfo.Uses[info.Memory] = builder.createCallUse(info.Memory, call);
                callInfo.Uses[info.StackPointer] = builder.createCallUse(info.StackPointer, call);
                callInfo.Uses[info.FramePointer] = builder.createCallUse(info.FramePointer, call);

                for (auto& [gv, gvObj] : info.Globals) {
                    callInfo.Defs[gvObj] = builder.createCallDef(gvObj, call);
                    callInfo.Uses[gvObj] = builder.createCallUse(gvObj, call);
                }
            }
        } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
            memory::RetUse* use = builder.createReturnUse(info.Memory, *ret);
            assert(info.ExitUses.count(info.Memory) == 0 && "There must be at most one return use!");
            
            info.ExitUses[info.Memory] = use;

            for (auto& [gv, gvObj] : info.Globals) {
                assert(info.ExitUses.count(gvObj) == 0 && "There must be at most one return use!");
                info.ExitUses[gvObj] = builder.createReturnUse(gvObj, *ret);
            }
        } else if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
            builder.createAllocaDef(info.Memory, *alloca);
            builder.createAllocaDef(info.StackPointer, *alloca);
        }
    }   
}

void FlatMemoryModel::handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    auto memSSA = this->getFunctionMemorySSA(*bb.getParent());

    for (MemoryObjectDef& def : memSSA->definitionAnnotationsFor(&bb)) {
        Variable* defVariable = ep.getVariableFor(&def);
        if (auto globalInit = llvm::dyn_cast<memory::GlobalInitializerDef>(&def)) {
            ExprPtr pointer = ep.getAsOperand(globalInit->getGlobalVariable());
            ExprPtr globalValue = this->handleGlobalInitializer(globalInit, pointer, ep);
            if (!ep.tryToEliminate(&def, defVariable, globalValue)) {
                ep.insertAssignment(defVariable, globalValue);
            }
        } else if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
            ExprPtr liveOnEntryInit = this->handleLiveOnEntry(liveOnEntry, ep);
            if (liveOnEntryInit != nullptr) {
                if (!ep.tryToEliminate(&def, defVariable, liveOnEntryInit)) {
                    ep.insertAssignment(defVariable, liveOnEntryInit);
                }
            }
        }
    }
}

ExprPtr FlatMemoryModel::handleLiveOnEntry(
    memory::LiveOnEntryDef* def,
    llvm2cfa::GenerationStepExtensionPoint& ep
) {
    llvm::Function* function = def->getParentBlock()->getParent();
    if (function->getName() != "main") {
        return MemoryModel::handleLiveOnEntry(def, ep);
    }

    auto& info = mFunctionInfo[function];

    if (def->getObject() == info.StackPointer || def->getObject() == info.FramePointer) {
        // Initialize the stack and frame pointers to the beginning of the stack.
        return this->ptrConstant(StackBegin);
    }

    return MemoryModel::handleLiveOnEntry(def, ep);
}

ExprPtr FlatMemoryModel::buildMemoryRead(
    gazer::Type& targetTy, unsigned size, ExprPtr array, ExprPtr pointer)
{
    if (mSettings.ints == IntRepresentation::BitVectors) {
        switch (targetTy.getTypeID()) {
            case Type::BvTypeID: {
                ExprPtr result = mExprBuilder->Read(array, pointer);
                for (unsigned i = 1; i < size; ++i) {
                    // FIXME: Little/big endian
                    result = mExprBuilder->BvConcat(
                        mExprBuilder->Read(array, this->pointerOffset(pointer, i)),
                        result
                    );
                }
                return result;
            }
            case Type::BoolTypeID: {
                ExprPtr result = mExprBuilder->NotEq(mExprBuilder->Read(array, pointer), mExprBuilder->BvLit8(0));
                for (unsigned i = 1; i < size; ++i) {
                    result = mExprBuilder->And(
                        mExprBuilder->NotEq(
                            mExprBuilder->Read(array, this->pointerOffset(pointer, i)),
                            mExprBuilder->BvLit8(0)
                        ),
                        result
                    );

                    return result;
                }
            }
            case Type::FloatTypeID: {
                // TODO: We will need a bitcast from bitvectors to floats.
                return mExprBuilder->Undef(targetTy);
            }
            default:
                // If it is not a convertible type, just undef it.
                return mExprBuilder->Undef(targetTy);
        }

        llvm_unreachable("Unhandled target type!");
    }

    if (mSettings.ints == IntRepresentation::Integers) {
        switch (targetTy.getTypeID()) {
            case Type::IntTypeID: {
                // Try to reconstruct the value from the integer operands.
                // To do so, we iterate over each cell, and use the following formula:
                //  x += (Mem[ptr + i] mod 256) * pow(2, i * 8)

                ExprPtr result = mExprBuilder->IntLit(0);
                for (unsigned i = 0; i < size; ++i) {
                    // FIXME: Little/big endian
                    result = mExprBuilder->Add(
                        mExprBuilder->Mul(
                            mExprBuilder->Mod(
                                mExprBuilder->Read(array, this->pointerOffset(pointer, i)), mExprBuilder->IntLit(256)
                            ),
                            mExprBuilder->IntLit(math::ipow(2, i * 8))
                        ),
                        result
                    );
                }

                return result;
            }
            case Type::BoolTypeID: {
                ExprPtr result = mExprBuilder->NotEq(mExprBuilder->Read(array, pointer), mExprBuilder->IntLit(0));
                for (unsigned i = 1; i < size; ++i) {
                    result = mExprBuilder->And(
                        mExprBuilder->NotEq(
                            mExprBuilder->Read(array, this->pointerOffset(pointer, i)),
                            mExprBuilder->IntLit(0)
                        ),
                        result
                    );

                    return result;
                }
            }
            default:
                // If it is not a convertible type, just undef it.
                return mExprBuilder->Undef(targetTy);
        }
    }

    llvm_unreachable("Unknown integer representation strategy!");
}

ExprPtr FlatMemoryModel::handleLoad(
    const llvm::LoadInst& load,
    ExprPtr pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    llvm::SmallVector<memory::LoadUse*, 1> annotations;
    this->getFunctionMemorySSA(*load.getFunction())->memoryAccessOfKind(&load, annotations);

    assert(annotations.size() == 1);
    MemoryObjectUse* use = annotations[0];
    MemoryObjectDef* def = use->getReachingDef();

    auto& info = mFunctionInfo[load.getFunction()];
    if (auto gvObj = info.Globals.lookup(load.getPointerOperand())) {
        // If the object is a partitioned global, just return its reaching definition.
        return ep.getAsOperand(def);
    }

    Type& loadTy = translateType(load.getType());
    ExprPtr array = UndefExpr::Get(this->getMemoryObjectType());
    
    assert(def != nullptr && "There must be a reaching definition for this load!");
    array = ep.getAsOperand(def);

    unsigned size = mDataLayout.getTypeAllocSize(load.getType());
    assert(size >= 1);

    return this->buildMemoryRead(loadTy, size, array, pointer);
}

void FlatMemoryModel::handleStore(
    const llvm::StoreInst& store,
    ExprPtr pointer,
    ExprPtr value,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    auto& info = mFunctionInfo[store.getFunction()];
    memory::MemorySSA* memSSA = this->getFunctionMemorySSA(*store.getFunction());

    auto annotations = memSSA->definitionAnnotationsFor(&store);

    if (annotations.begin() == annotations.end()) {
        return;
    }

    assert(std::next(annotations.begin(), 1) == annotations.end());

    MemoryObjectDef& def = *annotations.begin();
    Variable* defVariable = ep.getVariableFor(&def);

    if (auto gvObj = info.Globals.lookup(store.getPointerOperand())) {
        // This is a partitioned global variable, just write the given value into it.
        if (!ep.tryToEliminate(&def, defVariable, value)) {
            ep.insertAssignment(defVariable, value);
        }
        return;
    }

    // Write the values into the array, byte-by-byte.
    unsigned size = mDataLayout.getTypeAllocSize(store.getValueOperand()->getType());

    ExprPtr reachingDef = ep.getAsOperand(def.getReachingDef());
    ExprPtr array = reachingDef;

    if (auto bvTy = llvm::dyn_cast<BvType>(&value->getType())) {
        if (bvTy->getWidth() == 8) {
            array = mExprBuilder->Write(array, pointer, value);
        } else if (bvTy->getWidth() < 8) {
            array = mExprBuilder->Write(
                array, pointer, mExprBuilder->ZExt(value, BvType::Get(mContext, 8))
            );
        } else {
            for (unsigned i = 0; i < size; ++i) {
                array = mExprBuilder->Write(
                    array,
                    this->pointerOffset(pointer, i),
                    mExprBuilder->Extract(value, i * 8, 8)
                );
            }
        }
    } else if (value->getType().isBoolType()) {
        array = mExprBuilder->Write(
            array,
            pointer,
            mExprBuilder->Select(value, mExprBuilder->BvLit8(0x01), mExprBuilder->BvLit8(0x00))
        );
    } else {
        // Even with unknown/unhandled types, we know which bytes we modify -- we just
        // do not know the value.
        for (unsigned i = 0; i < size; ++i) {
            array = mExprBuilder->Write(
                array,
                this->pointerOffset(pointer, i),
                mExprBuilder->Undef(BvType::Get(mContext, 8))
            );
        }
    }

    if (!ep.tryToEliminate(&def, defVariable, array)) {
        ep.insertAssignment(defVariable, array);
    }
}

ExprPtr FlatMemoryModel::handleAlloca(
    const llvm::AllocaInst& alloc,
    llvm2cfa::GenerationStepExtensionPoint& ep
) {
    auto& info = mFunctionInfo[alloc.getFunction()];
    auto annot = this->getFunctionMemorySSA(*alloc.getFunction())->definitionAnnotationsFor(&alloc);

    auto spDef = std::find_if(annot.begin(), annot.end(), [&info](MemoryObjectDef& def) {
        return def.getObject() == info.StackPointer;
    });
    auto memDef = std::find_if(annot.begin(), annot.end(), [&info](MemoryObjectDef& def) {
        return def.getObject() == info.Memory;
    });

    assert(spDef != annot.end());
    assert(memDef != annot.end());

    unsigned size = mDataLayout.getTypeAllocSize(alloc.getAllocatedType());

    // This alloca returns the pointer to the current stack frame,
    // which is then advanced by the size of the allocated type.
    // We also clobber the relevant bytes of the memory array.
    ExprPtr ptr = ep.getAsOperand(spDef->getReachingDef());
    ep.insertAssignment(ep.getVariableFor(&*spDef), mExprBuilder->Add(
        ptr, this->ptrConstant(size)
    ));

    ExprPtr resArray = ep.getAsOperand(memDef->getReachingDef());
    for (unsigned i = 0; i < size; ++i) {
        resArray = mExprBuilder->Write(
            resArray,
            this->pointerOffset(ptr, i),
            mExprBuilder->Undef(BvType::Get(mContext, 8))
        );
    }

    Variable* memVar = ep.getVariableFor(&*memDef);
    if (!ep.tryToEliminate(&*memDef, memVar, resArray)) {
        ep.insertAssignment(memVar, resArray);
    }

    return ptr;
}

void FlatMemoryModel::handleCall(
    llvm::ImmutableCallSite call,
    llvm2cfa::GenerationStepExtensionPoint& callerEp,
    llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
    std::vector<VariableAssignment>& inputAssignments,
    std::vector<VariableAssignment>& outputAssignments,
    std::vector<VariableAssignment>& additionalAssignments)
{
    const llvm::Function* callee = call.getCalledFunction();
    if (callee == nullptr) {
        // TODO: Indirect calls.
        return;
    }

    auto& calleeInfo = mFunctionInfo[callee];
    auto& callerInfo = mFunctionInfo[call->getFunction()];
    auto& callInstInfo = callerInfo.Calls[call.getInstruction()];

    auto memSSA = this->getFunctionMemorySSA(*call->getFunction());

    auto callDefs = memSSA->definitionAnnotationsFor(call.getInstruction());
    auto callUses = memSSA->useAnnotationsFor(call.getInstruction());

    // Map memory call definitions to return uses
    outputAssignments.emplace_back(
        callerEp.getVariableFor(callInstInfo.Defs[callerInfo.Memory]),
        calleeEp.getOutputVariableFor(calleeInfo.ExitUses[calleeInfo.Memory]->getReachingDef())->getRefExpr()
    );

    // Map possible partitioned globals and insert output assignments
    for (auto [gv, obj] : callerInfo.Globals) {
        outputAssignments.emplace_back(
            callerEp.getVariableFor(callInstInfo.Defs[obj]),
            calleeEp.getOutputVariableFor(
                calleeInfo.ExitUses[calleeInfo.Globals[gv]]->getReachingDef()
            )->getRefExpr()
        );
    }

    // Map memory, stack and frame pointers
    for (auto [actual, formal] : std::initializer_list<std::pair<MemoryObject*, MemoryObject*>>{ 
        { callerInfo.Memory, calleeInfo.Memory },
        { callerInfo.StackPointer, calleeInfo.StackPointer },
        { callerInfo.FramePointer, calleeInfo.FramePointer }})
    {
        inputAssignments.emplace_back(
            calleeEp.getInputVariableFor(calleeInfo.EntryDefs[formal]),
            callerEp.getAsOperand(callInstInfo.Uses[actual]->getReachingDef())
        );
    }

    // Map partitioned globals
    for (auto [gv, obj] : callerInfo.Globals) {
        inputAssignments.emplace_back(
            calleeEp.getInputVariableFor(
                calleeInfo.EntryDefs[calleeInfo.Globals[gv]]
            ),
            callerEp.getAsOperand(callInstInfo.Uses[obj]->getReachingDef())
        );
    }
}

ExprPtr FlatMemoryModel::handleGetElementPtr(
    const llvm::GetElementPtrInst& gep,
    llvm::ArrayRef<ExprPtr> ops)
{
    assert(ops.size() == gep.getNumOperands());
    assert(ops.size() >= 2);

    ExprPtr addr = ops[0];
    for (unsigned i = 1; i < ops.size(); ++i) {
        llvm::Value* gepOperand = gep.getOperand(i);
        addr = mExprBuilder->Add(
            addr,
            mExprBuilder->Mul(ops[i], this->ptrConstant(mDataLayout.getTypeAllocSize(gepOperand->getType())))
        );
    }

    return addr;
}

auto gazer::CreateFlatMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<FlatMemoryModel>(context, settings, dl);
}