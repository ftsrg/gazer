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
///
/// This memory model declares three memory objects: Global, Stack and Heap.
///
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>

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

    struct FunctionInfo
    {
        MemoryObject* Stack;
        MemoryObject* Global;
        MemoryObject* Heap;

        llvm::DenseMap<MemoryObjectDef*, MemoryObjectDefInfo> Defs;
        llvm::DenseMap<const llvm::GlobalVariable*, ExprPtr> GlobalPointers;
        llvm::DenseMap<const llvm::Value*, MemoryObject*> KnownPointers;
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
        mExprBuilder = settings.simplifyExpr
                        ? CreateFoldingExprBuilder(context)
                        : CreateExprBuilder(context);
    }

public:
    void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint) override
    {}

    ExprPtr handlePointerCast(const llvm::CastInst& cast) override {
        return UndefExpr::Get(translateType(cast.getType()));
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

        MemoryObject* object = info.KnownPointers[value];

        if (object == nullptr) {
        }

        // TODO
        return UndefExpr::Get(this->getPointerType());
    }

    /// Translates the given LoadInst into an assignable expression.
    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    void handleStore(
        const llvm::StoreInst& store,
        const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep,
        std::vector<VariableAssignment>& assignments
    ) override;

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        const llvm::SmallVectorImpl<memory::AllocaDef*>& annotations
    ) override {
        return UndefExpr::Get(this->getPointerType());
    }

    /// Maps the given memory object to a memory object in function.
    void handleCall(
        llvm::ImmutableCallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        std::vector<VariableAssignment>& inputAssignments,
        std::vector<VariableAssignment>& outputAssignments,
        std::vector<VariableAssignment>& additionalAssignments
    ) override
    {
    }

    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops
    ) override {
        return UndefExpr::Get(this->getPointerType());
    }

    void handleBlock(
        const llvm::BasicBlock& bb,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override {}

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
        for (unsigned i = 0; i < cda->getNumElements(); ++i) {
            builder.addValue(
                IntLiteralExpr::Get(mContext, i),
                elements[i]
            );
        }

        return builder.build();
    }


    ExprPtr handleGlobalInitializer(
        memory::GlobalInitializerDef* def,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override
    {
        auto& info = mFunctionInfo[ep.getParent()];

        llvm::GlobalVariable* gv = def->getGlobalVariable();
        llvm::Value* initializer = gv->getInitializer();

        assert(def->getReachingDef() != nullptr);

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
                    mExprBuilder->Undef(BvType::Get(mContext, 8))
                );
            }
        }

        return array;
    }

protected:
    void initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder) override;

private:
    BvType& getPointerType() const { return BvType::Get(mContext, 32); }
    
    ArrayType& getMemoryObjectType() const {
        return ArrayType::Get(this->getPointerType(), BvType::Get(mContext, 8));
    }

    MemoryObject* trackPointerToSingleObject(FunctionInfo& info, const llvm::Value* value);

    ExprPtr pointerOffset(ExprPtr pointer, unsigned offset) {
        return mExprBuilder->Add(pointer, mExprBuilder->BvLit32(offset));
    }

private:
    llvm::DenseMap<llvm::Function*, FunctionInfo> mFunctionInfo;
    std::unique_ptr<ExprBuilder> mExprBuilder;
};

} // end anonymous namespace

void FlatMemoryModel::initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder)
{
    auto& info = mFunctionInfo[&function];
    // TODO: This should be more flexible.
    bool isEntryFunction = function.getName() == "main";

    // Each function inserts three memory objects: Global, Heap, and Stack.
    info.Stack = builder.createMemoryObject(
        StackBegin,
        this->getMemoryObjectType(),
        MemoryObject::UnknownSize,
        llvm::Type::getInt8Ty(function.getContext()),
        "Stack"
    );
    info.Global = builder.createMemoryObject(
        GlobalBegin,
        this->getMemoryObjectType(),
        MemoryObject::UnknownSize,
        llvm::Type::getInt8Ty(function.getContext()),
        "Global"
    );
    info.Heap = builder.createMemoryObject(
        HeapBegin,
        this->getMemoryObjectType(),
        MemoryObject::UnknownSize,
        llvm::Type::getInt8Ty(function.getContext()),
        "Heap"
    );

    builder.createLiveOnEntryDef(info.Stack);
    builder.createLiveOnEntryDef(info.Global);
    builder.createLiveOnEntryDef(info.Heap);

    // Handle global variables
    unsigned globalAddr = GlobalBegin;
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        info.GlobalPointers[&gv] = mExprBuilder->BvLit32(globalAddr);
        info.KnownPointers[&gv] = info.Global;

        unsigned siz = mDataLayout.getTypeAllocSize(gv.getType()->getPointerElementType());
        globalAddr += siz;

        if (isEntryFunction && gv.hasInitializer()) {
            builder.createGlobalInitializerDef(info.Global, &gv);
        }
    }

    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            MemoryObject* object = trackPointerToSingleObject(info, store->getPointerOperand());
            if (object != nullptr) {
                builder.createStoreDef(object, *store);
            } else {
                builder.createStoreDef(info.Stack, *store);
                builder.createStoreDef(info.Global, *store);
                builder.createStoreDef(info.Heap, *store);
            }
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            MemoryObject* object = trackPointerToSingleObject(info, load->getPointerOperand());

            if (object != nullptr) {
                builder.createLoadUse(object, *load);
            } else {
                builder.createLoadUse(info.Stack, *load);
                builder.createLoadUse(info.Global, *load);
                builder.createLoadUse(info.Heap, *load);
            }
        } else if (auto call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
            llvm::Function* callee = call->getCalledFunction();

            if (callee == nullptr) {
                // TODO: Indirect calls.
            }

            if (callee->getName().startswith("gazer.") || callee->getName().startswith("llvm.")) {
                // TODO: This may need to change in the case of some intrinsics (e.g. llvm.memcpy)
                continue;
            }

            // Currently we assume that function declarations which return a value do not modify
            // global variables.
            // FIXME: We should make this configurable.
            if (!callee->isDeclaration() || callee->getReturnType()->isVoidTy()) {
                builder.createCallDef(info.Global, call);
                builder.createCallUse(info.Global, call);
            }

            builder.createCallDef(info.Stack, call);
            builder.createCallDef(info.Heap, call);

            builder.createCallUse(info.Stack, call);
            builder.createCallUse(info.Heap, call);
        } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
            builder.createReturnUse(info.Global, *ret);
            builder.createReturnUse(info.Heap, *ret);
            // The stack should not be considered alive after a function exits.
        }
    }   
}

ExprPtr FlatMemoryModel::handleLoad(
    const llvm::LoadInst& load,
    const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
    ExprPtr pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    Type& loadTy = translateType(load.getType());
    ExprPtr array = UndefExpr::Get(this->getMemoryObjectType());
    if (annotations.size() == 1) {
        MemoryObjectUse* use = annotations[0];
        MemoryObjectDef* def = use->getReachingDef();
        assert(def != nullptr && "There must be a reaching definition for this load!");

        array = ep.getAsOperand(def);

        unsigned size = mDataLayout.getTypeAllocSize(load.getType());
        assert(size >= 1);

        ExprPtr result;
        if (loadTy.isBvType()) {
            result = mExprBuilder->Read(array, pointer);
            for (unsigned i = 1; i < size; ++i) {
                // TODO: Little/big endian
                result = mExprBuilder->BvConcat(
                    mExprBuilder->Read(array, this->pointerOffset(pointer, i)),
                    result
                );
            }
        } else if (loadTy.isBoolType()) {
            result = mExprBuilder->NotEq(
                mExprBuilder->Read(array, pointer),
                mExprBuilder->BvLit8(0)
            );
        } else if (loadTy.isFloatType()) {
            // TODO: We will need a bitcast from bitvectors to floats.
            return UndefExpr::Get(loadTy);
        } else {
            return UndefExpr::Get(loadTy);
        }
        
        assert(result->getType() == loadTy);
        return result;
    } else {
        // TODO
    }

    return UndefExpr::Get(translateType(load.getType()));
}

void FlatMemoryModel::handleStore(
    const llvm::StoreInst& store,
    const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
    ExprPtr pointer,
    ExprPtr value,
    llvm2cfa::GenerationStepExtensionPoint& ep,
    std::vector<VariableAssignment>& assignments)
{
    if (annotations.size() == 0) {
        return;
    }

    if (annotations.size() == 1) {
        MemoryObjectDef* def = annotations[0];
        Variable* defVariable = ep.getVariableFor(def);

        ExprPtr reachingDef = ep.getAsOperand(def->getReachingDef());

        unsigned size = mDataLayout.getTypeAllocSize(store.getValueOperand()->getType());
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

        assignments.emplace_back(defVariable, array);
    }
}

MemoryObject* FlatMemoryModel::trackPointerToSingleObject(FunctionInfo& info, const llvm::Value* value)
{
    assert(value->getType()->isPointerTy());

    llvm::SmallVector<const llvm::Value*, 4> wl;
    wl.push_back(value);

    while (!wl.empty()) {
        const llvm::Value* ptr = wl.pop_back_val();

        MemoryObject* object = info.KnownPointers.lookup(ptr);
        if (object != nullptr) {
            return object;
        } else if (auto bitcast = llvm::dyn_cast<llvm::BitCastInst>(ptr)) {
            wl.push_back(bitcast->getOperand(0));
        } else if (auto select = llvm::dyn_cast<llvm::SelectInst>(ptr)) {
            wl.push_back(select->getOperand(1));
            wl.push_back(select->getOperand(2));
        } else {
            // We cannot track this pointer any further.
            return nullptr;
        }
    }

    return nullptr;
}

auto gazer::CreateFlatMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<FlatMemoryModel>(context, settings, dl);
}