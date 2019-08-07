#include <llvm/IR/Instructions.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>

namespace gazer
{

class CheckRegistry;

/// A check is a special kind of an LLVM pass, which marks instrunctions 
/// with pre- or postconditions which must be always true.
class Check : public llvm::ModulePass
{
    friend class CheckRegistry;
public:
    Check(char& id)
        : ModulePass(id)
    {}

    Check(const Check&) = delete;
    Check& operator=(const Check&) = delete;

    virtual bool runOnModule(llvm::Module& module) final;

    /// Returns this check's name. Names should be descriptive,
    /// but must not contain whitespaces,
    /// e.g.: "assert-fail", "div-by-zero", "int-overflow"
    virtual llvm::StringRef getCheckName() const = 0;

    /// Returns a user-friendly error description on why this particular
    /// check failed. Such descriptions should be short and simple, e.g.
    /// "Assertion failure", "Division by zero", or "Integer overflow".
    virtual llvm::StringRef getErrorDescription() const = 0;

    /// Marks the given function's instructions with required
    /// pre- and postconditions.
    virtual bool mark(llvm::Function& function) = 0;

protected:

    /// Creates an error block with a gazer.error_code(i16 code) call and a terminating unreachable instruction.
    llvm::BasicBlock* createErrorBlock(
        llvm::Function& function, llvm::Value* errorCode,
        const llvm::Twine& name = "", llvm::Instruction* location = nullptr
    );

    CheckRegistry& getRegistry() const;

private:
    void setCheckRegistry(CheckRegistry* registry);

private:
    unsigned mErrorCode = 0;
    CheckRegistry* mRegistry;
};

class CheckRegistry
{
public:
    static constexpr char ErrorFunctionName[] = "gazer.error_code";
public:
    CheckRegistry() = default;

    CheckRegistry(const CheckRegistry&) = delete;
    CheckRegistry& operator=(const CheckRegistry&) = delete;

    static llvm::FunctionCallee GetErrorFunction(llvm::Module& module);
    static llvm::FunctionCallee GetErrorFunction(llvm::Module* module) {
        return GetErrorFunction(*module);
    }

    static llvm::FunctionType* GetErrorFunctionType(llvm::LLVMContext& context);

public:
    void add(Check* check);
    void registerPasses(llvm::legacy::PassManager& pm);

    unsigned getErrorCode(char& id);

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
    llvm::StringMap<Check*> mCheckNames;

    // Start with 1, zero stands for unknown errors.
    unsigned mErrorCodeCnt = 1;
};

} // end namespace gazer
