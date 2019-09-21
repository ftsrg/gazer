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
        llvm::Function& function, const llvm::Twine& name = "", llvm::Instruction* location = nullptr
    );

    CheckRegistry& getRegistry() const;

private:
    void setCheckRegistry(CheckRegistry& registry);

private:
    CheckRegistry* mRegistry;
};

class CheckViolation
{
public:
    CheckViolation(Check* check, llvm::DebugLoc location)
        : mCheck(check), mLocation(location)
    {
        assert(check != nullptr);
    }

    CheckViolation(const CheckViolation&) = default;
    CheckViolation& operator=(const CheckViolation&) = default;

    Check* getCheck() const { return mCheck; }
    llvm::DebugLoc getDebugLoc() const { return mLocation; }

private:
    Check* mCheck;
    llvm::DebugLoc mLocation;
};

class CheckRegistry
{
public:
    static constexpr char ErrorFunctionName[] = "gazer.error_code";
public:
    CheckRegistry(llvm::LLVMContext& context)
        : mLlvmContext(context)
    {}

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

    /// Creates a new check violation with a unique error code for a given check and location.
    /// \return An LLVM value representing the error code.
    llvm::Value* createCheckViolation(Check* check, llvm::DebugLoc loc);

    std::string messageForCode(unsigned ec);
private:
    llvm::LLVMContext& mLlvmContext;
    std::vector<Check*> mChecks;
    llvm::DenseMap<unsigned, CheckViolation> mCheckMap;
    llvm::StringMap<Check*> mCheckNames;

    // Start with 1, zero stands for unknown errors.
    unsigned mErrorCodeCnt = 1;
};

} // end namespace gazer
