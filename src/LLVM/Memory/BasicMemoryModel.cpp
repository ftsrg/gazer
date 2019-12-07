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
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>

#include <boost/container_hash/hash.hpp>

using namespace gazer;
using namespace gazer::memory;

namespace
{

class CallParamScope
{
    enum ScopeKind
    {
        Scope_Unknown,
        Scope_Global,
        Scope_PtrArgument,
    };
public:
    CallParamScope()
        : mScope(Scope_Unknown), mSource(nullptr)
    {}

    explicit CallParamScope(llvm::GlobalVariable* gv)
        : mScope(Scope_Global), mSource(gv)
    {}

    explicit CallParamScope(llvm::Argument* arg)
        : mScope(Scope_PtrArgument), mSource(arg)
    {}

    llvm::GlobalVariable* getGlobalVariableSource() const {
        if (mScope == Scope_Global) {
            return std::get<llvm::GlobalVariable*>(mSource);
        }

        return nullptr;
    }
    llvm::Argument* getArgumentSource() const {
        if (mScope == Scope_PtrArgument) {
            return std::get<llvm::Argument*>(mSource);
        }

        return nullptr;
    }

    bool operator==(const CallParamScope& rhs) const {
        return mScope == rhs.mScope && mSource == rhs.mSource;
    }

    size_t getHashCode() const {
        return std::hash<decltype(mSource)>{}(mSource);
    }

private:
    ScopeKind mScope;
    std::variant<std::nullptr_t, llvm::GlobalVariable*, llvm::Argument*> mSource;
};

struct CallParamScopeHash
{
    std::size_t operator()(const CallParamScope& scope) const { return scope.getHashCode(); }
};

class BasicMemoryModel : public MemoryModel
{
    struct MemoryObjectFormalParamMapping
    {
        CallParamScope scope;
        MemoryObjectDef* entryDef;
        MemoryObjectUse* exitUse;
    };

    struct MemoryObjectActualParamMapping
    {
        CallParamScope scope;
        llvm::SmallVector<memory::CallUse*, 1> inputCandidates;
        llvm::SmallVector<memory::CallDef*, 1> outputCandidates;
    };

    struct FunctionMemoryInfo
    {
        llvm::DenseMap<llvm::Value*, MemoryObject*> Objects;
        llvm::DenseMap<MemoryObject*, llvm::Value*> ObjectToValue;
        llvm::DenseMap<memory::LiveOnEntryDef*, CallParamScope> EntryDefs;
        llvm::DenseMap<memory::CallUse*, CallParamScope> CallUses;
        llvm::DenseMap<memory::CallDef*, CallParamScope> CallDefs;
        std::unordered_map<CallParamScope, MemoryObjectFormalParamMapping, CallParamScopeHash> FormalParams;
        llvm::DenseMap<
            llvm::CallSite,
            std::unordered_map<CallParamScope, MemoryObjectActualParamMapping, CallParamScopeHash>
        > ActualParams;

        void addObject(llvm::Value* source, MemoryObject* object)
        {
            Objects[source] = object;
            ObjectToValue[object] = source;
        }
    };
public:
    using MemoryModel::MemoryModel;

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloca,
        const llvm::SmallVectorImpl<memory::AllocaDef*>& annotations
    ) override;

    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    void handleCall(
        llvm::ImmutableCallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        std::vector<VariableAssignment>& inputAssignments,
        std::vector<VariableAssignment>& outputAssignments,
        std::vector<VariableAssignment>& additionalAssignments
    ) override;

    void handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override;

    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops
    ) override
    {
        assert(ops.size() == gep.getNumOperands());
        assert(ops[0]->getType().isArrayType()
            && "Pointers must be represented as arrays in a BasicMemoryModel!");

        auto& info = mFunctions[gep.getFunction()];

        ExprPtr base = ops[0];
        ExprPtr offset = IntLiteralExpr::Get(mContext, 0);

        for (unsigned i = 1; i < ops.size(); ++i) {
            offset = AddExpr::Create(offset, ops[i]);
        }

        return ArrayWriteExpr::Create(
            base, IntLiteralExpr::Get(mContext, 1), offset
        );
    }

    ExprPtr handlePointerCast(const llvm::CastInst& cast) override
    {
        return UndefExpr::Get(this->getPointerType());
    }

    ExprPtr handlePointerValue(const llvm::Value* value, llvm::Function& function) override
    {
        auto& info = mFunctions[&function];

        llvm::SmallPtrSet<MemoryObject*, 4> objects;
        bool hasObject = this->trackPointerToMemoryObject(info, value, objects);

        if (hasObject && objects.size() == 1) {
            return this->makePointer(*objects.begin(), 0);
        }

        return UndefExpr::Get(this->getPointerType());
    }

    void handleStore(
        const llvm::StoreInst& store,
        const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep,
        std::vector<VariableAssignment>& assignments
    ) override;

    Type& handlePointerType(const llvm::PointerType* type) override
    {
        return this->getPointerType();
    }

    ArrayType& handleArrayType(const llvm::ArrayType* type) override
    {
        return ArrayType::Get(
            IntType::Get(mContext),
            this->translateType(type->getArrayElementType())
        );
    }
    
    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda,
        llvm::ArrayRef<ExprRef<LiteralExpr>> elements
    ) override
    {
        assert(elements.size() == cda->getNumElements());

        ArrayLiteralExpr::Builder builder(this->handleArrayType(cda->getType()));
        for (unsigned i = 0; i < cda->getNumElements(); ++i) {
            builder.addValue(
                IntLiteralExpr::Get(mContext, i),
                elements[i]
            );
        }

        return builder.build();
    }

    ExprRef<ArrayLiteralExpr> makePointer(MemoryObject* object, unsigned offset)
    {
        return ArrayLiteralExpr::Get(
            this->getPointerType(),
            {
                { IntLiteralExpr::Get(mContext, 0), IntLiteralExpr::Get(mContext, object->getId())},
                { IntLiteralExpr::Get(mContext, 1), IntLiteralExpr::Get(mContext, offset) }
            },
            IntLiteralExpr::Get(mContext, 0)
        );
    }

    void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint) override;

protected:
    void initializeFunction(llvm::Function& function, MemorySSABuilder& builder) override;

private:
    ArrayType& getPointerType() const {
        return ArrayType::Get(IntType::Get(mContext), IntType::Get(mContext));
    }

    bool trackPointerToMemoryObject(
        FunctionMemoryInfo& function,
        const llvm::Value* value,
        llvm::SmallPtrSetImpl<MemoryObject*>& candidates
    );
    void flattenType(llvm::Type* type, llvm::SmallVectorImpl<llvm::Type*>& flattened);

    ExprPtr ptrForMemoryObject(MemoryObject* object)
    {
        return this->makePointer(object, 0);
    }

    template<class Iterator>
    ExprPtr disambiguatePointerDefs(
        Iterator begin, Iterator end, ExprPtr pointer, llvm2cfa::GenerationStepExtensionPoint& ep
    ) {
        assert(begin != end && "Must pass a non-empty range!");
        ExprPtr expr = UndefExpr::Get(ep.getVariableFor(*begin)->getType());

        for (auto it = begin; it != end; ++it) {
            MemoryObjectDef* def = *it;
            Variable* defVariable = ep.getVariableFor(def);
            assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");

            expr = SelectExpr::Create(
                EqExpr::Create(pointer, ptrForMemoryObject(def->getObject())),
                ep.getAsOperand(def),
                expr
            );
        }

        return expr;
    }

private:
    llvm::DenseMap<const llvm::Function*, FunctionMemoryInfo> mFunctions;
    unsigned mId = 0;
};

} // end anonymous namespace

bool BasicMemoryModel::trackPointerToMemoryObject(
    FunctionMemoryInfo& function,
    const llvm::Value* value,
    llvm::SmallPtrSetImpl<MemoryObject*>& candidates
)
{
    assert(value->getType()->isPointerTy());

    llvm::SmallVector<const llvm::Value*, 4> wl;
    wl.push_back(value);

    while (!wl.empty()) {
        const llvm::Value* ptr = wl.pop_back_val();

        MemoryObject* object = function.Objects.lookup(ptr);
        if (object != nullptr) {
            candidates.insert(object);
        } else if (auto bitcast = llvm::dyn_cast<llvm::BitCastInst>(ptr)) {
            wl.push_back(bitcast->getOperand(0));
        } else if (auto select = llvm::dyn_cast<llvm::SelectInst>(ptr)) {
            wl.push_back(select->getOperand(1));
            wl.push_back(select->getOperand(2));
        } else {
            // We cannot track this pointer any further.
            return false;
        }
    }

    return true;
}

void BasicMemoryModel::flattenType(llvm::Type* type, llvm::SmallVectorImpl<llvm::Type*>& flattened)
{
    // TODO
}

static inline memory::CallDef* getOrCreateCallDefFor(
    MemorySSABuilder& builder, MemoryObject* object,
    llvm::CallSite call, llvm::SmallDenseMap<MemoryObject*, memory::CallDef*, 4>& defMap
) {
    if (auto callDef = defMap.lookup(object)) {
        return callDef;
    }

    auto def = builder.createCallDef(object, call);
    defMap[object] = def;

    return def;
}

static inline memory::CallUse* getOrCreateCallUseFor(
    MemorySSABuilder& builder, MemoryObject* object,
    llvm::CallSite call, llvm::SmallDenseMap<MemoryObject*, memory::CallUse*, 4>& useMap
) {
    if (auto callUse = useMap.lookup(object)) {
        return callUse;
    }

    auto use = builder.createCallUse(object, call);
    useMap[object] = use;

    return use;
}

void BasicMemoryModel::initializeFunction(llvm::Function& function, MemorySSABuilder& builder)
{
    auto& currentObjects = mFunctions[&function];
    unsigned tmp = 0;

    // TODO: This should be more flexible.
    bool isEntryFunction = function.getName() == "main";
    llvm::SmallPtrSet<llvm::Value*, 32> allocSites;

    // Each function will have a memory object made from this global variable.
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        auto gvTy = gv.getType()->getPointerElementType();

        gazer::Type* objectType;

        if (gvTy->isSingleValueType() && !gvTy->isVectorTy()) {
            objectType = &this->translateType(gvTy);
        } else if (gvTy->isArrayTy()) {
            objectType = &ArrayType::Get(
                IntType::Get(mContext),
                this->translateType(gvTy->getArrayElementType())
            );
        } else {
            llvm_unreachable("Unknown LLVM type!");
        }

        MemoryObject* object = builder.createMemoryObject(
            mId++,
            *objectType,
            getDataLayout().getTypeAllocSize(gvTy),
            gvTy,
            gv.getName()
        );

        if (isEntryFunction) {
            builder.createGlobalInitializerDef(
                object, gv.hasInitializer() ? gv.getInitializer() : nullptr
            );
        } else {
            memory::LiveOnEntryDef* liveOnEntry = builder.createLiveOnEntryDef(object);
            CallParamScope scope(&gv);
            currentObjects.EntryDefs[liveOnEntry] = scope;
            currentObjects.FormalParams[scope].entryDef = liveOnEntry;
        }
        currentObjects.addObject(&gv, object);
        allocSites.insert(&gv);
    }

    for (llvm::Argument& arg : function.args()) {
        llvm::Type* argTy = arg.getType();
        if (argTy->isPointerTy()) {
            std::string name = arg.hasName() ? arg.getName().str() : ("arg_" + std::to_string(tmp++));

            auto object = builder.createMemoryObject(
                mId++,
                this->translateType(argTy->getPointerElementType()),
                getDataLayout().getTypeAllocSize(argTy->getPointerElementType()),
                argTy->getPointerElementType(),
                name
            );

            memory::LiveOnEntryDef* liveOnEntry = builder.createLiveOnEntryDef(object);
            CallParamScope scope(&arg);
            currentObjects.EntryDefs[liveOnEntry] = scope;
            currentObjects.FormalParams[scope].entryDef = liveOnEntry;
            currentObjects.addObject(&arg, object);
            allocSites.insert(&arg);
        }
    }

    // Add all alloca instructions
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
            llvm::Type* allocatedTy = alloca->getType()->getPointerElementType();
            std::string name = alloca->hasName() ? alloca->getName().str() : ("alloca_" + std::to_string(tmp++));

            auto object = builder.createMemoryObject(
                mId++,
                this->translateType(allocatedTy),
                getDataLayout().getTypeAllocSize(allocatedTy),
                allocatedTy,
                name
            );

            builder.createAllocaDef(object, *alloca);
            currentObjects.addObject(alloca, object);
            allocSites.insert(alloca);
        }
    }

    // Now, walk over all instructions and search for pointer operands
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        llvm::SmallPtrSet<MemoryObject*, 4> candidates;

        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            bool hasValidObject = trackPointerToMemoryObject(
                currentObjects, store->getPointerOperand(), candidates
            );
            if (!hasValidObject) {
                // We could not track this pointer to known origins,
                // we must clobber all memory objects in order to be safe.
                for (auto& entry : currentObjects.Objects) {
                    builder.createStoreDef(entry.second, *store);
                }
            } else {
                // Otherwise, just create a definition for all possibly modified objects.
                for (MemoryObject* object : candidates) {
                    builder.createStoreDef(object, *store);
                }
            }
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            bool hasValidObject = trackPointerToMemoryObject(
                currentObjects,
                load->getPointerOperand(),
                candidates
            );

            // If no valid object was traceable, we could not track the origins of the pointer.
            // We will not insert any annotations, and the LoadInst will be translated to
            // an undef value. Otherwise, insert a use for all possibly read memory objects.
            if (hasValidObject) {
                for (MemoryObject* object : candidates) {
                    builder.createLoadUse(object, *load);
                }
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

            auto& paramsMapping = currentObjects.ActualParams[call];

            llvm::SmallDenseMap<MemoryObject*, memory::CallDef*, 4> defSet;
            llvm::SmallDenseMap<MemoryObject*, memory::CallUse*, 4> useSet;

            for (unsigned i = 0; i < callee->arg_size(); ++i) {
                llvm::Argument* formalArg = &*(std::next(callee->arg_begin(), i));
                if (formalArg->getType()->isPointerTy()) {
                    CallParamScope scope(formalArg);
                    llvm::Value* arg = call->getArgOperand(i);
                    bool hasValidObject = trackPointerToMemoryObject(
                        currentObjects, arg, candidates
                    );
                    if (!hasValidObject) {
                        // The memory object could not be resolved, we have to clobber all of them.
                        for (auto& [val, obj] : currentObjects.Objects) {
                            memory::CallDef* def = getOrCreateCallDefFor(builder, obj, call, defSet);
                            memory::CallUse* use = getOrCreateCallUseFor(builder, obj, call, useSet);
                            paramsMapping[scope].outputCandidates.push_back(def);
                            paramsMapping[scope].inputCandidates.push_back(use);
                        }
                    } else {
                        for (MemoryObject* object : candidates) {
                            memory::CallDef* def = getOrCreateCallDefFor(builder, object, call, defSet);
                            memory::CallUse* use = getOrCreateCallUseFor(builder, object, call, useSet);
                            paramsMapping[scope].outputCandidates.push_back(def);
                            paramsMapping[scope].inputCandidates.push_back(use);
                        }
                    }
                }
            }

            // Currently we assume that function declarations which return a value do not modify
            // global variables.
            // FIXME: We should make this configurable.
            if (!callee->isDeclaration() || callee->getReturnType()->isVoidTy()) {
                for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
                    // Pass global variables to the functions
                    MemoryObject* object = currentObjects.Objects[&gv];
                    CallParamScope scope(&gv);

                    memory::CallDef* def = getOrCreateCallDefFor(builder, object, call, defSet);
                    memory::CallUse* use = getOrCreateCallUseFor(builder, object, call, useSet);
                    paramsMapping[scope].outputCandidates.push_back(def);
                    paramsMapping[scope].inputCandidates.push_back(use);
                }
            }
        } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
            // A use annotation on a return instruction indicates that the memory object
            // should be considered alive after the function returns.
            // In this memory model, all memory objects except the ones allocated with
            // an alloca will be considered alive on return.
            for (auto& [allocation, object] : currentObjects.Objects) {
                if (object->hasEntryDef() && llvm::isa<memory::LiveOnEntryDef>(object->getEntryDef())) {
                    MemoryObjectDef* def = object->getEntryDef();
                    MemoryObjectUse* use = builder.createReturnUse(object, *ret);

                    // Find the corresponding formal param
                    // FIXME: We should probably use a reverse map instead of find_if
                    auto& formalParams = currentObjects.FormalParams;
                    auto result = std::find_if(formalParams.begin(), formalParams.end(), [def](auto& entry) {
                        return entry.second.entryDef == def;
                    });
                    assert(result != formalParams.end()
                        && "A memory object with an entry def must be present in the formal parameters map!");

                    MemoryObjectFormalParamMapping& mapping = result->second;
                    mapping.exitUse = use;
                }
            }
        }
    }
}

void BasicMemoryModel::handleCall(
    llvm::ImmutableCallSite cs,
    llvm2cfa::GenerationStepExtensionPoint& callerEP,
    llvm2cfa::AutomatonInterfaceExtensionPoint& targetEP,
    std::vector<VariableAssignment>& inputAssignments,
    std::vector<VariableAssignment>& outputAssignments,
    std::vector<VariableAssignment>& additionalAssignments
)
{
    // FIXME: This is a nasty workaround, needed because ImmutableCallSite does not have
    //  DenseMapInfo, nor it is possible to implement it ourselves. We should remove
    //  this as soon as we can use ImmutableCallSite's as DenseMap keys.
    llvm::CallSite call(const_cast<llvm::Instruction*>(cs.getInstruction()));
    assert(call.getCalledFunction() != nullptr);

    FunctionMemoryInfo& callerInfo = mFunctions[call.getParent()->getParent()];
    FunctionMemoryInfo& calleeInfo = mFunctions[call.getCalledFunction()];
    assert(callerInfo.ActualParams.count(call) != 0);

    llvm::SmallDenseMap<Variable*, llvm::SmallVector<ExprPtr, 1>, 4> outputCandidates;

    // Try to map formal parameters to actual ones.

    // A map which will contain the output auxiliary assignment.
    // Each element in this map is a [auxVariable, condition] pair.
    // As an example, if the map contains { [aux1, ptr = tmp1], [aux2, ptr = tmp2] } for
    // some definition D, then the following aux assignment will be generated:
    //  D_new := Select(Eq(ptr, tmp), aux1, Select(Eq(ptr, tmp2), aux2, D)
    llvm::DenseMap<memory::CallDef*, std::vector<std::pair<Variable*, ExprPtr>>> auxAssignments;

    for (auto& [scope, formalMapping] : calleeInfo.FormalParams) {
        if (callerInfo.ActualParams[call].count(scope) == 0) {
            continue;
        }

        MemoryObjectActualParamMapping& actualMapping = callerInfo.ActualParams[call][scope];
        assert(!actualMapping.inputCandidates.empty());
        assert(!actualMapping.outputCandidates.empty());

        Variable* inputVar = targetEP.getInputVariableFor(formalMapping.entryDef);
        ExprPtr actualInput;
        if (actualMapping.inputCandidates.size() == 1) {
            MemoryObjectDef* reachingDef = actualMapping.inputCandidates[0]->getReachingDef();
            actualInput = callerEP.getAsOperand(reachingDef);
        } else if (llvm::Argument* arg = scope.getArgumentSource()) {
            assert(arg->getType()->isPointerTy());

            llvm::Value* argOperand = call.getArgument(arg->getArgNo());
            ExprPtr ptr = callerEP.getAsOperand(argOperand);

            actualInput = UndefExpr::Get(inputVar->getType());

            for (memory::CallUse* candidate : actualMapping.inputCandidates) {
                MemoryObjectDef* reachingDef = candidate->getReachingDef();
                actualInput = SelectExpr::Create(
                    EqExpr::Create(ptr, this->ptrForMemoryObject(reachingDef->getObject())),
                    callerEP.getAsOperand(reachingDef),
                    actualInput
                );
            }
        } else {
            // We could not resolve the memory object, insert an undef expression.
            actualInput = UndefExpr::Get(inputVar->getType());
        }

        // Insert the input assignment
        inputAssignments.emplace_back(inputVar, actualInput);

        // Now handle the outputs
        MemoryObjectDef* outputDef = formalMapping.exitUse->getReachingDef();
        Variable* outputVariable = targetEP.getOutputVariableFor(outputDef);

        if (actualMapping.outputCandidates.size() == 1) {
            MemoryObjectDef* callDef = actualMapping.outputCandidates[0];
            Variable* definedVariable = callerEP.getVariableFor(callDef);
            outputAssignments.emplace_back(definedVariable, outputVariable->getRefExpr());
        } else if (llvm::Argument* arg = scope.getArgumentSource()) {
            assert(arg->getType()->isPointerTy());

            llvm::Value* argOperand = call.getArgument(arg->getArgNo());
            ExprPtr ptr = callerEP.getAsOperand(argOperand);

            // We have to be a little tricky here: we introduce a new variable, which
            // will get the value of whatever comes from the called function. The final
            // assignment will be on a separate transition, where an additional assignment
            // will disambiguate based on the pointer value.
            std::string auxName = "__call_output_"
                + std::to_string(outputDef->getObject()->getId())
                + "_" + std::to_string(arg->getArgNo());

            Variable* aux = callerEP.createAuxiliaryVariable(auxName, outputVariable->getType());
            outputAssignments.emplace_back(aux, outputVariable->getRefExpr());

            actualInput = UndefExpr::Get(inputVar->getType());

            for (memory::CallDef* candidate : actualMapping.outputCandidates) {
                Variable* definedVariable = callerEP.getVariableFor(candidate);

                auxAssignments[candidate].emplace_back(
                    aux,
                    EqExpr::Create(ptr, this->ptrForMemoryObject(candidate->getObject()))
                );
            }
        } else {
            // We could not resolve the memory object, insert an undef expression.
            actualInput = UndefExpr::Get(inputVar->getType());
        }
    }

    for (auto& [callDef, vec] : auxAssignments) {
        ExprPtr expr = callerEP.getAsOperand(callDef->getReachingDef());
        for (auto& [auxVariable, condition] : vec) {
            expr = SelectExpr::Create(
                condition, auxVariable->getRefExpr(), expr
            );
        }
    }
}

void BasicMemoryModel::declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint)
{
}

void BasicMemoryModel::handleStore(
    const llvm::StoreInst& store,
    const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
    ExprPtr pointer,
    ExprPtr value,
    llvm2cfa::GenerationStepExtensionPoint& ep,
    std::vector<VariableAssignment>& assignments
) {
    if (annotations.size() == 0) {
        // If this store has no annotations, do nothing.
        return;
    }

    if (annotations.size() == 1) {
        MemoryObjectDef* def = annotations[0];
        Variable* defVariable = ep.getVariableFor(def);
        assert(defVariable != nullptr
            && "Each memory object definition should map to a variable in the CFA!");

        ExprPtr lhs;
        switch (def->getObject()->getKind()) {
            case MemoryObject::Kind_Array: {
                ExprPtr prev = ep.getAsOperand(def->getReachingDef());
                lhs = ArrayWriteExpr::Create(
                    prev,
                    ArrayReadExpr::Create(pointer, IntLiteralExpr::Get(mContext, 1)),
                    value
                );
                break;
            }
            case MemoryObject::Kind_Scalar: {
                lhs = value;
                break;
            }
            default:
                lhs = UndefExpr::Get(defVariable->getType());
                break;
        }        

        assert(defVariable->getType() == lhs->getType());

        if (!ep.tryToEliminate(def, defVariable, lhs)) {
            assignments.emplace_back(defVariable, lhs);
        }
        return;
    }

    // If a single store may clobber multiple memory objects, we disambiguate at 'runtime',
    // using the pointer values.
    for (memory::StoreDef* def : annotations) {
        Variable* defVariable = ep.getVariableFor(def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        assert(defVariable->getType() == value->getType());

        MemoryObjectDef* reachingDef = def->getReachingDef();
        assert(reachingDef != nullptr && "Store without a previous reaching definition?");

        auto select = SelectExpr::Create(
            EqExpr::Create(pointer, ptrForMemoryObject(def->getObject())),
            value,
            ep.getAsOperand(reachingDef)
        );

        if (!ep.tryToEliminate(def, defVariable, value)) {
            assignments.emplace_back(defVariable, value);
        }
    }
}

ExprPtr BasicMemoryModel::handleLoad(
    const llvm::LoadInst& load,
    const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
    ExprPtr pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep
) {
    if (annotations.size() == 0) {
        // If no memory access annotations are available for this load,
        // then the memory model was unable to resolve it.
        // We will over-approximate, and return an undef expression.
        return UndefExpr::Get(translateType(load.getType()));
    }

    if (annotations.size() == 1) {
        MemoryObjectUse* use = annotations[0];
        MemoryObjectDef* def = use->getReachingDef();
        assert(def != nullptr && "There must be a reaching definition for this load!");

        return ep.getAsOperand(def);
    }

    // If this load may be clobbered by multiple definitions, we disambiguate using the pointer values.
    ExprPtr expr = UndefExpr::Get(translateType(load.getType()));

    for (memory::LoadUse* use : annotations) {
        MemoryObjectDef* reachingDef = use->getReachingDef();
        Variable* defVariable = ep.getVariableFor(reachingDef);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");

        expr = SelectExpr::Create(
            EqExpr::Create(pointer, ptrForMemoryObject(reachingDef->getObject())),
            ep.getAsOperand(reachingDef),
            expr
        );
    }

    return expr;
}

void BasicMemoryModel::handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    /*
    auto& memSSA = getFunctionMemorySSA(*bb.getParent());
    for (MemoryObjectDef& def : memSSA.definitionAnnotationsFor(&bb)) {
        Variable* defVariable = ep.getVariableFor(&def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        if (auto globalInit = llvm::dyn_cast<GlobalInitializerDef>(&def)) {
            ep.insertAssignment({defVariable, ep.operand(globalInit->getInitializer())});
        }
    } */
}

ExprPtr BasicMemoryModel::handleAlloca(
    const llvm::AllocaInst& alloca, const llvm::SmallVectorImpl<memory::AllocaDef*>& annotations
) {
    assert(annotations.size() && "An alloca inst must define exactly one memory object!");

    MemoryObject* object = annotations[0]->getObject();
    return ptrForMemoryObject(object);
}

auto gazer::CreateBasicMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<BasicMemoryModel>(context, settings, dl);
}
