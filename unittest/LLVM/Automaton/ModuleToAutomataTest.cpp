
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/AsmParser/Parser.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/CallGraph.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace gazer {
    extern llvm::cl::opt<bool> NoElimVars;
}

namespace
{

::testing::AssertionResult VariableListContains(
    llvm::iterator_range<Cfa::var_iterator> vars,
    std::initializer_list<std::pair<std::string, gazer::Type*>> names
) {
    for (auto& pair : names) {
        if (std::none_of(vars.begin(), vars.end(), [&pair](Variable& v) {
            return v.getName() == pair.first && v.getType() == *pair.second;
        })) {
            return ::testing::AssertionFailure()
                << "Expected " << pair.first << " with type "
                << pair.second->getName() << " in the given variable list";
        }
    }

    return ::testing::AssertionSuccess();
}

class ModuleToAutomataTest : public ::testing::Test
{
protected:
    llvm::LLVMContext llvmContext;
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module;
    std::vector<std::unique_ptr<llvm::DominatorTree>> dominators;
    std::vector<std::unique_ptr<llvm::LoopInfo>> loops;
    std::unordered_map<llvm::Function*, llvm::LoopInfo*> loopInfoMap;
public:
    ModuleToAutomataTest(const char* moduleStr)
        : module(llvm::parseAssemblyString(moduleStr, error, llvmContext))
    {
        assert(module != nullptr && "Failed to construct LLVM module.");

        for (llvm::Function& function : *module) {
            if (!function.isDeclaration()) {
                auto& dt = dominators.emplace_back(std::make_unique<llvm::DominatorTree>(function));
                auto& loop = loops.emplace_back(std::make_unique<llvm::LoopInfo>(*dt));

                loopInfoMap[&function] = loop.get();
            }
        }
    }
};

class BasicModuleToAutomataTest : public ModuleToAutomataTest
{
public:
    BasicModuleToAutomataTest()
        : ModuleToAutomataTest(R"ASM(
declare i32 @__VERIFIER_nondet_int()

define i32 @calculate(i32 %x, i32 %y) {
    %sum = add nsw i32 %x, %y
    ret i32 %sum
}

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.body ]
    %sum = phi i32 [ 0, %entry ], [ %s, %loop.body ]
    %cond = icmp slt i32 %i, %limit
    br i1 %cond, label %loop.body, label %loop.end
loop.body:
    %a = call i32 @__VERIFIER_nondet_int()
    %s = call i32 @calculate(i32 %a, i32 %sum)
    %i1 = add nsw i32 %i, 1
    br label %loop.header
loop.end:
    ret i32 %sum
}
)ASM") {}
};

TEST_F(BasicModuleToAutomataTest, CanCreateAllAutomata)
{
    GazerContext context;
    llvm::DenseMap<llvm::Value*, Variable*> vmap;
    llvm::DenseMap<Location*, llvm::BasicBlock*> blocks;
    auto system = translateModuleToAutomata(*module, loopInfoMap, context, vmap, blocks);

    ASSERT_EQ(system->getNumAutomata(), 3);

    Cfa* main = system->getAutomatonByName("main");
    ASSERT_TRUE(main != nullptr);

    Cfa* calculate = system->getAutomatonByName("calculate");
    ASSERT_TRUE(calculate != nullptr);

    Cfa* loop = system->getAutomatonByName("main/loop.header");
    ASSERT_TRUE(loop != nullptr);

    EXPECT_EQ(main->getNumInputs(), 0);
    EXPECT_EQ(main->getNumLocals(), 3); // limit, RET_VAL, sum
    EXPECT_EQ(main->getNumOutputs(), 1); // RET_VAL

    EXPECT_TRUE(VariableListContains(main->locals(), {
        { "main/limit", &BvType::Get(context, 32) },
        { "main/sum", &BvType::Get(context, 32) }
    }));

    EXPECT_EQ(calculate->getNumInputs(), 2); // x, y
    EXPECT_EQ(calculate->getNumLocals(), 2); // sum, RET_VAL
    EXPECT_EQ(calculate->getNumOutputs(), 1); // RET_VAL

    EXPECT_TRUE(VariableListContains(calculate->inputs(), {
        { "calculate/x", &BvType::Get(context, 32) },
        { "calculate/y", &BvType::Get(context, 32) }
    }));
    EXPECT_TRUE(VariableListContains(calculate->locals(), {
        { "calculate/sum", &BvType::Get(context, 32) }
    }));

    EXPECT_EQ(loop->getNumInputs(), 3); // i, sum, limit
    EXPECT_EQ(loop->getNumLocals(), 4); // cond, a, s, i1
    EXPECT_EQ(loop->getNumOutputs(), 1); // sum

    EXPECT_TRUE(VariableListContains(loop->inputs(), {
        { "main/loop.header/i", &BvType::Get(context, 32) },
        { "main/loop.header/sum", &BvType::Get(context, 32) },
        { "main/loop.header/limit", &BvType::Get(context, 32) }
    }));
    EXPECT_TRUE(VariableListContains(loop->locals(), {
        { "main/loop.header/cond", &BoolType::Get(context) },
        { "main/loop.header/a", &BvType::Get(context, 32) },
        { "main/loop.header/s", &BvType::Get(context, 32) },
        { "main/loop.header/i1", &BvType::Get(context, 32) },
    }));
    EXPECT_TRUE(VariableListContains(loop->outputs(), {
        { "main/loop.header/sum", &BvType::Get(context, 32) }
    }));

    EXPECT_TRUE(VariableListContains(calculate->inputs(), {
        { "calculate/x", &BvType::Get(context, 32) },
        { "calculate/y", &BvType::Get(context, 32) }
    }));
    EXPECT_TRUE(VariableListContains(calculate->locals(), {
        { "calculate/sum", &BvType::Get(context, 32) }
    }));
}

class PostTestLoopTest : public ModuleToAutomataTest
{
public:
    PostTestLoopTest()
        : ModuleToAutomataTest(R"ASM(
declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.header ]
    %i1 = add nsw i32 %i, 1
    %cond = icmp slt i32 %i1, %limit
    br i1 %cond, label %loop.header, label %loop.end
loop.end:
    ret i32 0
}
)ASM") {}
};

TEST_F(PostTestLoopTest, CanTransformPostTestLoop)
{
    GazerContext context;
    llvm::DenseMap<llvm::Value*, Variable*> vmap;
    llvm::DenseMap<Location*, llvm::BasicBlock*> blocks;
    auto system = translateModuleToAutomata(*module, loopInfoMap, context, vmap, blocks);

    Cfa* main = system->getAutomatonByName("main");
    ASSERT_TRUE(main != nullptr);

    Cfa* loop = system->getAutomatonByName("main/loop.header");
    ASSERT_TRUE(loop != nullptr);
}

class LoopWithMultipleExitsTest : public ModuleToAutomataTest
{
public:
    LoopWithMultipleExitsTest()
        : ModuleToAutomataTest(R"ASM(
declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %limit = call i32 @__VERIFIER_nondet_int()
    br label %loop.header
loop.header:
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.calculate ]
    %sum = phi i32 [ 0, %entry ], [ %s, %loop.calculate ]
    %cond = icmp slt i32 %i, %limit
    br i1 %cond, label %loop.body, label %loop.end
loop.body:
    %a = call i32 @__VERIFIER_nondet_int()
    %error.cond = icmp slt i32 %a, 0
    br i1 %error.cond, label %error, label %loop.calculate
loop.calculate:
    %i1 = add nsw i32 %i, 1
    %s = add nsw i32 %sum, %a
    %c = call i32 @__VERIFIER_nondet_int()
    %c1 = trunc i32 %c to i1
    br i1 %c1, label %loop.end, label %loop.header
loop.end:
    ret i32 %sum
error:
    ret i32 1
}
)ASM") {}
};

TEST_F(LoopWithMultipleExitsTest, CanTransformLoopWithMultipleExits)
{
    GazerContext context;
    llvm::DenseMap<llvm::Value*, Variable*> vmap;
    llvm::DenseMap<Location*, llvm::BasicBlock*> blocks;
    auto system = translateModuleToAutomata(*module, loopInfoMap, context, vmap, blocks);

    Cfa* main = system->getAutomatonByName("main");
    ASSERT_TRUE(main != nullptr);

    Cfa* loop = system->getAutomatonByName("main/loop.header");
    ASSERT_TRUE(loop != nullptr);
}

class NestedLoopsTest : public ModuleToAutomataTest
{

public:
    NestedLoopsTest()
        : ModuleToAutomataTest(R"ASM(
declare i32 @__VERIFIER_nondet_int()

define i32 @main() {
entry:
    %c1 = call i32 @__VERIFIER_nondet_int()
    %c2 = call i32 @__VERIFIER_nondet_int()
    br label %loop.header

loop.header:
    %result = phi i32 [ 0, %entry ], [ %result1, %loop.latch ]
    %i = phi i32 [ 0, %entry ], [ %i1, %loop.latch ]
    %loop.cond = icmp slt i32 %i, %c1
    br i1 %loop.cond, label %loop.body, label %exit

loop.body:
    %x = call i32 @__VERIFIER_nondet_int()
    br label %nested.header

nested.header:
    %s  = phi i32 [ 0, %loop.body ], [ %s2, %nested.body ]
    %j  = phi i32 [ 0, %loop.body ], [ %j1, %nested.body ]
    %nested.cond = icmp slt i32 %j, %c2
    br i1 %nested.cond, label %nested.body, label %loop.latch
nested.body:
    %y  = call i32 @__VERIFIER_nondet_int()
    %s1 = add nsw i32 %x, %y
    %s2 = add nsw i32 %s, %s1
    %j1 = add nsw i32 %j, 1
    br label %nested.header

loop.latch:
    %result1 = add nsw i32 %result, %s
    %i1 = add nsw i32 %i, 1
    br label %loop.header

exit:
    ret i32 %result
}
)ASM") {}
};

TEST_F(NestedLoopsTest, CanTransformLoopWithNestedLoop)
{
    // Turn off variable elimination
    NoElimVars.setValue(true);

    GazerContext context;
    llvm::DenseMap<llvm::Value*, Variable*> vmap;
    llvm::DenseMap<Location*, llvm::BasicBlock*> blocks;
    auto system = translateModuleToAutomata(*module, loopInfoMap, context, vmap, blocks);

    Cfa* main = system->getAutomatonByName("main");
    ASSERT_TRUE(main != nullptr);

    EXPECT_EQ(main->getNumInputs(), 0);
    EXPECT_EQ(main->getNumLocals(), 4); // c1, c2, result, RET_VAL
    EXPECT_EQ(main->getNumOutputs(), 1); // RET_VAL

    EXPECT_TRUE(VariableListContains(main->locals(), {
        { "main/c1", &BvType::Get(context, 32) },
        { "main/c2", &BvType::Get(context, 32) },
        { "main/result", &BvType::Get(context, 32) },
        { "main/RET_VAL", &BvType::Get(context, 32) }
    }));

    Cfa* loop = system->getAutomatonByName("main/loop.header");
    ASSERT_TRUE(loop != nullptr);

    EXPECT_EQ(loop->getNumInputs(), 4); // c1, c2, result, i
    EXPECT_EQ(loop->getNumLocals(), 5); // loop.cond, x, s, result1, i1
    EXPECT_EQ(loop->getNumOutputs(), 1); // result

    EXPECT_TRUE(VariableListContains(loop->inputs(), {
        { "main/loop.header/c1", &BvType::Get(context, 32) },
        { "main/loop.header/c2", &BvType::Get(context, 32) },
        { "main/loop.header/result", &BvType::Get(context, 32) },
        { "main/loop.header/i", &BvType::Get(context, 32) },
    }));
    EXPECT_TRUE(VariableListContains(loop->locals(), {
        { "main/loop.header/loop.cond", &BoolType::Get(context) },
        { "main/loop.header/x", &BvType::Get(context, 32) },
        { "main/loop.header/s", &BvType::Get(context, 32) },
        { "main/loop.header/result1", &BvType::Get(context, 32) },
        { "main/loop.header/i1", &BvType::Get(context, 32) }
    }));
    EXPECT_TRUE(VariableListContains(loop->outputs(), {
        { "main/loop.header/result", &BvType::Get(context, 32) }
    }));

    Cfa* nested = system->getAutomatonByName("main/nested.header");
    ASSERT_TRUE(nested != nullptr);

    EXPECT_EQ(nested->getNumInputs(), 4); // c2, x, s, j
    EXPECT_EQ(nested->getNumLocals(), 5); // nested.cond, j1, s1, s2, y
    EXPECT_EQ(nested->getNumOutputs(), 1); // s

    EXPECT_TRUE(VariableListContains(nested->inputs(), {
        { "main/nested.header/c2", &BvType::Get(context, 32) },
        { "main/nested.header/x", &BvType::Get(context, 32) },
        { "main/nested.header/s", &BvType::Get(context, 32) },
        { "main/nested.header/j", &BvType::Get(context, 32) },
    }));
    EXPECT_TRUE(VariableListContains(nested->locals(), {
        { "main/nested.header/nested.cond", &BoolType::Get(context) },
        { "main/nested.header/j1", &BvType::Get(context, 32) },
        { "main/nested.header/s1", &BvType::Get(context, 32) },
        { "main/nested.header/s2", &BvType::Get(context, 32) },
        { "main/nested.header/y", &BvType::Get(context, 32) }
    }));
    EXPECT_TRUE(VariableListContains(nested->outputs(), {
        { "main/nested.header/s", &BvType::Get(context, 32) }
    }));

}

} // end anonymous namespace