## Checks

During verification, Gazer instruments the input program with so-called _checks_.
Checks usually insert pre- or postconditions for a given instruction (such as the second operand of a division cannot be zero). If these conditions fail, an error call is inserted.

Checks are managed through the `CheckRegistry` class, which defines an error code for each check. These are used to identify the violated check in the case of a verification failure.

### Registering checks

`CheckRegistry` is used to register new checks into the verification workflow, and used in conjunction with LLVM's `PassManager`.
To register a check, simply add its factory to a `FrontendConfig` instance:

```cpp
FrontendConfig& config = /*... */;

// Create a check a custom check class
config.registerCheck("my-check-name", [](ClangOptions& options) {
    return std::make_unique<MyOwnCheckClass>();
});
```

The factory function receives a `ClangOptions` instance where it can set certain Clang flags for the frontend compilation stage.

Checks may be enabled through the command line, using the check names set in the `registerCheck` function.
As an example, the command line option `-checks="div-by-zero,my-check-name"` enables the check registered above along with the default division-by-zero check.

If you are using `LLVMFrontend`, the `registerVerificationPipeline` function runs all requested checks on the input module.

If not, you must register and enable the passes manually through the `registerPassess` function of `CheckRegistry`:

```cpp
CheckRegistry& checks = /* ... */;
std::unique_ptr<llvm::legacy::PassManager> pm = /* ... */;
checks.registerPasses(*pm);
// If you add other passes to the pass manager, they will be executed after check instrumentation
pm->add(/* ... */);
// Run all passes
pm->run(module);
```

### Writing custom checks

By implementation, checks are just [LLVM passes](http://llvm.org/docs/WritingAnLLVMPass.html#introduction-what-is-a-pass) with special additions for traceability support.
To write your own check, create a new subclass deriving from `Check`:

```cpp
class DivisionByZeroCheck : public gazer::Check
{
public:
    static char ID;

    DivisionByZeroCheck()
        : Check(ID)
    {}

    virtual bool mark(llvm::Function&) override;
    virtual llvm::StringRef getErrorDescription() const override;
};
```

The virtual function `mark` is used to insert the error calls into a function, while `getErrorDescription` should return a short,
user-friendly error message if the check is violated (such as "Assertion failure", "Division by zero", etc.).

Error calls are represented with Gazer's `gazer.error_code` function.
It requires a single operand, which is an error code unique for the given check.
A check's error code value can be retrieved through the CheckRegistry:

```cpp
llvm::LLVMContext& context = function.getContext();
llvm::Value* ec = getRegistry().getErrorCodeValue(context, ID);
```

With the error code, you can implement your check and place the pre- or postconditions and error calls.

```cpp
char DivisionByZeroCheck::ID;

llvm::StringRef DivisionByZeroCheck::getErrorDescription() const {
    return "Divison by zero";
}

bool DivisionByZeroCheck::mark(llvm::Function& function)
{
    // Find the division instructions and collect them.
    std::vector<Instruction*> divs = /* ... */

    if (divs.empty()) {
        // We did not modify the function.
        return false;
    }

    llvm::LLVMContext& context = function.getContext();

    // Create a builder for inserting the preconditions.
    llvm::IRBuilder<> builder(context);

    unsigned divCnt = 0;
    for (llvm::Instruction* inst : divs) {
        // Create a block for the error call.
        // As no instruction should be executed after an error call,
        // an UnreachableInst is used to terminate these error blocks.
        BasicBlock* errorBB = this->createErrorBlock(
            function,
            getRegistry().getErrorCodeValue(context, ID),
            "error.divzero" + std::to_string(divCnt++)
        );

        llvm::BasicBlock* bb = inst->getParent();
        llvm::Value* rhs = inst->getOperand(1);

        // Create the precondition check
        builder.SetInsertPoint(inst);
        auto icmp = builder.CreateICmpNE(
            rhs, builder.getInt(llvm::APInt(
                rhs->getType()->getIntegerBitWidth(), 0
            ))
        );

        // Split the basic block to insert a jump based on the precondition check.
        // If the check is successful, control will jump to the division instruction. If not, it jumps to the error call.
        llvm::BasicBlock* newBB = bb->splitBasicBlock(inst);
        
        builder.ClearInsertionPoint();
        // Replace the new original block's terminator with the conditional jump
        llvm::ReplaceInstWithInst(
            bb->getTerminator(),
            builder.CreateCondBr(icmp, newBB, errorBB)
        );
    }

    return true;
}
```

**NOTE:** Verification algorithms are not required to find a solution for each registered check seperately.
They often combine all error calls (for example with an `CombineErrorCalls` pass) into a single one, which makes them stop after finding the first violated check.
