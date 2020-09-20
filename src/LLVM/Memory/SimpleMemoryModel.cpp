//
// Created by rx7 on 2020. 09. 10..
//

#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/Core/Expr/ExprBuilder.h"

using namespace gazer;

namespace {

class SimpleMemoryModel : public MemoryModel,
                          public MemoryInstructionHandler,
                          public MemoryTypeTranslator
{
public:
    MemoryInstructionHandler& getMemoryInstructionHandler(llvm::Function& function) override
    {
        return *this;
    }

    MemoryTypeTranslator& getMemoryTypeTranslator() override
    {
        return *this;
    }

    ExprPtr handlePointerCast(const llvm::CastInst& cast, const ExprPtr& origPtr) override
    {
        return mBuilder->Undef(mTypes.get(cast.getType()));
    }

    gazer::Type& handlePointerType(const llvm::PointerType* type) override
    {
        return mTypes.get(type->getPointerElementType());
    }

    ExprPtr handlePointerValue(const llvm::Value* value) override
    {
        return mBuilder->Undef(mTypes.get(value->getType()));
    }

    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda,
        llvm::ArrayRef<ExprRef<LiteralExpr>> elems) override
    {
        return mBuilder->Undef(mTypes.get(cda->getType()));
    }

    ExprPtr handleGetElementPtr(const llvm::GetElementPtrInst& gep, llvm::ArrayRef<ExprPtr> ops)
        override
    {
        return mBuilder->Undef(mTypes.get(gep.getType()));
    }

    ExprPtr handleAlloca(const llvm::AllocaInst& alloc, llvm2cfa::GenerationStepExtensionPoint& ep)
        override
    {
        Variable* var =
            ep.createAuxiliaryVariable(alloc.getName(), mTypes.get(alloc.getAllocatedType()));
        variableMapping.insert({(llvm::Value*) &alloc, (Variable * &&) var});
        return var->getRefExpr();
    }

    ExprPtr
        handleLoad(const llvm::LoadInst& load, llvm2cfa::GenerationStepExtensionPoint& ep) override
    {
        const auto* ptr = load.getPointerOperand();
        assert(
            variableMapping.count(const_cast<llvm::Value*>(ptr)) != 0
            && "Variable not present in variable mapping");

        // copy value to temporary
        auto* var = ep.createAuxiliaryVariable(load.getName(), mTypes.get(load.getType()));
        const auto* globalVar = variableMapping.find(const_cast<llvm::Value*>(ptr))->second;
        ep.insertAssignment(var, globalVar->getRefExpr());
        return var->getRefExpr();
    }

    void handleStore(const llvm::StoreInst& store, llvm2cfa::GenerationStepExtensionPoint& ep)
        override
    {
        const auto* ptr = store.getPointerOperand();
        assert(
            variableMapping.count(const_cast<llvm::Value*>(ptr)) != 0
            && "Variable not present in variable mapping");

        auto* globalVar = variableMapping.find(const_cast<llvm::Value*>(ptr))->second;
        ep.insertAssignment(globalVar, ep.getAsOperand(store.getValueOperand()));
    }

    void declareGlobalVariables(llvm::Module& module, llvm2cfa::GlobalVarDeclExtensionPoint& ep)
        override
    {
        for (auto& global : module.globals()) {
            auto* var = ep.createGlobal(&global, mTypes.get(global.getValueType()));
            variableMapping.emplace(&global, var);
        }
    }

    gazer::Type& handleArrayType(const llvm::ArrayType* type) override
    {
        return mTypes.get(type->getElementType());
    }

    SimpleMemoryModel(
        GazerContext& context,
        const LLVMFrontendSettings& settings,
        llvm::Module& module)
        : MemoryTypeTranslator(context), mTypes(*this, settings), mBuilder(CreateFoldingExprBuilder(context))
    {}

    void handleCall(
        llvm::CallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
        llvm::SmallVectorImpl<VariableAssignment>& outputAssignments) override
    {}

    ExprPtr isValidAccess(llvm::Value* ptr, const ExprPtr& expr) override
    {
        return mBuilder->True();
    }

private:
    /** Stores alloca'd variables and globals */
    std::unordered_map<llvm::Value*, Variable*> variableMapping;

    LLVMTypeTranslator mTypes;
    std::unique_ptr<ExprBuilder> mBuilder;
};

} // namespace

std::unique_ptr<MemoryModel> gazer::CreateSimpleMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    llvm::Module& module
) {
    return std::make_unique<SimpleMemoryModel>(context, settings, module);
}
