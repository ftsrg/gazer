#include <llvm/IR/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>

namespace gazer
{

/**
 * A check is a special kind of an LLVM pass, which marks instrunctions 
 * with pre- or postconditions which must be always true.
 */
class Check : public llvm::ModulePass
{
    friend class CheckRegistry;

    enum ConditionType { Precondition, Postcondition, Fail };

    struct Mark
    {
        llvm::Instruction* instruction;
        ConditionType type;
        std::vector<llvm::Value*> conditions;
    };
public:
    Check(char& id)
        : ModulePass(id)
    {}

    Check(const Check&) = delete;
    Check& operator=(const Check&) = delete;

    virtual bool runOnModule(llvm::Module& module) final;

    /**
     * Returns a user-friendly error description on why this particular
     * check failed. Such descriptions should be short and simple, e.g.
     * "Assertion failure", "Division by zero", or "Integer overflow".
     */
    virtual std::string getErrorName() const = 0;

    /**
     * Marks the given function's instructions with required
     * pre- and postconditions.
     */
    virtual bool mark(llvm::Function& function) = 0;

protected:

    /**
     * Creates an error block with a gazer.error() call and a terminating unreachable instruction.
     */
    llvm::BasicBlock* createErrorBlock
        (llvm::Function& function, llvm::Value* errorCode, const llvm::Twine& name = "");

private:
    unsigned mErrorCode = 0;
    //llvm::DenseMap<llvm::Instruction*, llvm::Value*> mPreConditions;
    //llvm::DenseMap<llvm::Instruction*, llvm::Value*> mPostConditions;
};

class CheckRegistry
{
public:
    void add(Check* check);
    void registerPasses(llvm::legacy::PassManager& pm);

    static llvm::Constant* GetErrorFunction(llvm::Module& module);
    static llvm::Constant* GetErrorFunction(llvm::Module* module) {
        return GetErrorFunction(*module);
    }

    static llvm::FunctionType* GetErrorFunctionType(llvm::LLVMContext& context);

    static CheckRegistry& GetInstance() {
        return Instance;
    }

    unsigned getErrorCode(char& id)
    {
        const void* idPtr = static_cast<const void*>(&id);
        auto result = mErrorCodes.find(idPtr);
        
        assert(result != mErrorCodes.end()
            && "Attempting to get code for an unregistered check type");

        return result->second;
    }

    llvm::Value* getErrorCodeValue(llvm::LLVMContext& context, char& id)
    {
        return llvm::ConstantInt::get(
            llvm::Type::getInt16Ty(context), llvm::APInt(16, getErrorCode(id))
        );
    }

    template<class CheckT>
    unsigned getErrorCode() {
        return getErrorCode(CheckT::ID);
    }

    template<class CheckT>
    llvm::Value* getErrorCodeValue(llvm::LLVMContext& context)
    {
        unsigned code = getErrorCode<CheckT>();
        return llvm::ConstantInt::get(
            llvm::Type::getInt16Ty(context), llvm::APInt(16, code)
        );
    }

    std::string messageForCode(unsigned ec);

private:
    std::vector<Check*> mChecks;
    llvm::DenseMap<const void*, unsigned> mErrorCodes;
    llvm::DenseMap<unsigned, Check*> mCheckMap;

    // Start with 1, zero stands for unknown errors.
    unsigned mErrorCodeCnt = 1;

    static CheckRegistry Instance;
};

namespace checks
{

Check* CreateAssertionFailCheck();
Check* CreateDivisionByZeroCheck();
Check* CreateIntegerOverflowCheck();

} // end namespace checks

} // end namespace gazer