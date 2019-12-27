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
#include "gazer/Support/Math.h"

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

    struct CallInfo
    {
        memory::CallUse* MemoryUse;
        memory::CallUse* StackPtrUse;
        memory::CallUse* FramePtrUse;

        memory::CallDef* MemoryDef;
    };

    struct FunctionInfo
    {
        MemoryObject* Memory;
        MemoryObject* StackPointer;
        MemoryObject* FramePointer;

        memory::RetUse* MemExitUse;
        memory::LiveOnEntryDef* MemEntryDef;
        memory::LiveOnEntryDef* StackPtrEntryDef;
        memory::LiveOnEntryDef* FramePtrEntryDef;

        llvm::DenseMap<const llvm::GlobalVariable*, ExprPtr> GlobalPointers;
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

    info.MemEntryDef = builder.createLiveOnEntryDef(info.Memory);
    info.StackPtrEntryDef = builder.createLiveOnEntryDef(info.StackPointer);
    info.FramePtrEntryDef = builder.createLiveOnEntryDef(info.FramePointer);

    // Handle global variables
    unsigned globalAddr = GlobalBegin;
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        info.GlobalPointers[&gv] = this->ptrConstant(globalAddr);

        unsigned siz = mDataLayout.getTypeAllocSize(gv.getType()->getPointerElementType());
        globalAddr += siz;

        if (isEntryFunction && gv.hasInitializer()) {
            builder.createGlobalInitializerDef(info.Memory, &gv);
        }
    }

    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            builder.createStoreDef(info.Memory, *store);
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            builder.createLoadUse(info.Memory, *load);
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

                callInfo.MemoryDef = builder.createCallDef(info.Memory, call);
                callInfo.MemoryUse = builder.createCallUse(info.Memory, call);
                callInfo.StackPtrUse = builder.createCallUse(info.StackPointer, call);
                callInfo.FramePtrUse = builder.createCallUse(info.FramePointer, call);
            }
        } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
            memory::RetUse* use = builder.createReturnUse(info.Memory, *ret);
            assert(info.MemExitUse == nullptr && "There must be at most one return use!");
            info.MemExitUse = use;
        } else if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
            builder.createAllocaDef(info.Memory, *alloca);
            builder.createAllocaDef(info.StackPointer, *alloca);
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

        llvm_unreachable("Unknown integer representation!");
    }
}

ExprPtr FlatMemoryModel::handleLoad(
    const llvm::LoadInst& load,
    ExprPtr pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{
    llvm::SmallVector<memory::LoadUse*, 1> annotations;
    this->getFunctionMemorySSA(*load.getFunction())->memoryAccessOfKind(&load, annotations);

    Type& loadTy = translateType(load.getType());
    ExprPtr array = UndefExpr::Get(this->getMemoryObjectType());
    if (annotations.size() == 1) {
        MemoryObjectUse* use = annotations[0];
        MemoryObjectDef* def = use->getReachingDef();
        assert(def != nullptr && "There must be a reaching definition for this load!");

        array = ep.getAsOperand(def);

        unsigned size = mDataLayout.getTypeAllocSize(load.getType());
        assert(size >= 1);

        return this->buildMemoryRead(loadTy, size, array, pointer);
    }

    return UndefExpr::Get(translateType(load.getType()));
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

    ExprPtr reachingDef = ep.getAsOperand(def.getReachingDef());

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

    assert(std::distance(callDefs.begin(), callDefs.end()) == 1
        && "A call instruction should define only the Memory array!");
    assert(callDefs.begin()->getObject() == callerInfo.Memory);
    
    assert(std::distance(callUses.begin(), callUses.end()) == 3
        && "A call instruction should only use the Memory array, Stack and Frame pointer objects!");

    assert(calleeInfo.MemExitUse != nullptr
        && "There must be a single memory exit use!");

    // Map the definition to the memory object return use
    outputAssignments.emplace_back(
        callerEp.getVariableFor(&*callDefs.begin()),
        calleeEp.getOutputVariableFor(calleeInfo.MemExitUse->getReachingDef())->getRefExpr()
    );

    // Map call uses to entry definitions
    inputAssignments.emplace_back(
        calleeEp.getInputVariableFor(calleeInfo.MemEntryDef),
        callerEp.getAsOperand(callInstInfo.MemoryUse->getReachingDef())
    );
    inputAssignments.emplace_back(
        calleeEp.getInputVariableFor(calleeInfo.StackPtrEntryDef),
        callerEp.getAsOperand(callInstInfo.StackPtrUse->getReachingDef())
    );
    inputAssignments.emplace_back(
        calleeEp.getInputVariableFor(calleeInfo.FramePtrEntryDef),
        callerEp.getAsOperand(callInstInfo.FramePtrUse->getReachingDef())
    );
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