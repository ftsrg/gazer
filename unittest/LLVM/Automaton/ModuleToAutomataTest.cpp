
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Analysis/MemoryObject.h"
#include "gazer/ADT/StringUtils.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/AsmParser/Parser.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Support/MemoryBuffer.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace gazer::unittest
{
    extern std::string ResourcesPath;
}

namespace
{

using Settings = ModuleToAutomataSettings;

std::string getResourceFile(std::string name)
{
    auto path = gazer::unittest::ResourcesPath + "/Automaton/" + name;
    auto file = llvm::MemoryBuffer::getFile(path);

    if (!file) {
        llvm::errs() << "Could not open file: " << file.getError().message() << "\n";
        return "";
    }

    return file.get()->getBuffer().str();
}

struct ModuleToCfaTestModel
{
    std::string mName;
    std::string mInput;
    std::string mExpected;
    ModuleToAutomataSettings mSettings;
};

std::ostream& operator<<(std::ostream& os, const ModuleToCfaTestModel& model)
{
    return os << "<" << model.mName << ":" << model.mInput << ",settings=" << model.mSettings.toString() << ">";
}

class ModuleToCfaTest : public testing::TestWithParam<ModuleToCfaTestModel>
{
public:
    using VariableListT = std::vector<std::pair<std::string, gazer::Type*>>;
protected:
    llvm::LLVMContext llvmContext;
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module;
    std::vector<std::unique_ptr<llvm::DominatorTree>> dominators;
    std::vector<std::unique_ptr<llvm::LoopInfo>> loops;
    llvm::DenseMap<llvm::Function*, llvm::LoopInfo*> loopInfoMap;

    GazerContext context;
    std::unique_ptr<MemoryModel> memoryModel = nullptr;

    llvm::DenseMap<llvm::Value*, Variable*> vmap;
    llvm::DenseMap<Location*, llvm::BasicBlock*> blocks;
    
protected:
    std::unique_ptr<AutomataSystem> createSystemFromModule(
        std::string input, ModuleToAutomataSettings settings, MemoryModel* memoryModel = nullptr
    ) {
        auto path = gazer::unittest::ResourcesPath + "/Automaton/" + input;
        auto buff = llvm::MemoryBuffer::getFile(path);

        module = llvm::parseAssemblyFile(path, error, llvmContext);
        assert(module != nullptr && "Failed to construct LLVM module.");

        for (llvm::Function& function : *module) {
            if (!function.isDeclaration()) {
                auto& dt = dominators.emplace_back(std::make_unique<llvm::DominatorTree>(function));
                auto& loop = loops.emplace_back(std::make_unique<llvm::LoopInfo>(*dt));

                loopInfoMap[&function] = loop.get();
            }
        }

        if (memoryModel == nullptr) {
            this->memoryModel.reset(new DummyMemoryModel(context));
        } else {
            this->memoryModel.reset(memoryModel);
        }

        return translateModuleToAutomata(
            *module, settings, loopInfoMap, context, *this->memoryModel, vmap, blocks
        );
    }
};

TEST_P(ModuleToCfaTest, TestWithoutMemoryModel)
{
    ModuleToCfaTestModel model = GetParam();
    auto system = createSystemFromModule(model.mInput, model.mSettings);

    std::string buff;
    llvm::raw_string_ostream rso(buff);
    system->print(rso);

    //llvm::errs() << rso.str();
    //for (auto& cfa : *system) {
    //    cfa.view();
    //}

    auto expected = getResourceFile(model.mExpected);

    if (expected == "") {
        // The file was not found, dump the current contents.
        llvm::errs() << "Expected file was not found. Actual output:\n";
        llvm::errs() << rso.str();
    }

    EXPECT_EQ(expected, rso.str());
}

std::vector<ModuleToCfaTestModel> GetTestModels()
{
    auto simpleConfig = Settings{
        Settings::ElimVars_Off, Settings::Loops_Recursion,
        Settings::Ints_UseBv, Settings::Floats_UseFpa,
        /*simplifyExpr=*/false
    };
    auto elimVarsDefaultConfig = Settings{
        Settings::ElimVars_Normal, Settings::Loops_Recursion,
        Settings::Ints_UseBv, Settings::Floats_UseFpa,
        /*simplifyExpr=*/false
    };
    auto elimVarsAggressiveConfig = Settings{
        Settings::ElimVars_Aggressive, Settings::Loops_Recursion,
        Settings::Ints_UseBv, Settings::Floats_UseFpa,
        /*simplifyExpr=*/false
    };

    return {
        { "AddFunc_Simple", "AddFuncTest.ll", "AddFuncTest_Simple.cfa", simpleConfig },
        { "AddFunc_ElimVarsNormal", "AddFuncTest.ll", "AddFuncTest_ElimVars.cfa", elimVarsDefaultConfig },
        { "AddFunc_ElimVarsAggressive", "AddFuncTest.ll", "AddFuncTest_ElimVars.cfa", elimVarsAggressiveConfig },
        { "LoopTest_Simple", "LoopTest.ll", "LoopTest_Simple.cfa", simpleConfig },
        { "LoopTest_ElimVarsNormal", "LoopTest.ll", "LoopTest_ElimVarsDefault.cfa", elimVarsDefaultConfig },
        { "LoopTest_ElimVarsAggressive", "LoopTest.ll", "LoopTest_ElimVarsDefault.cfa", elimVarsAggressiveConfig },
        { "NestedLoops_Simple", "NestedLoops.ll", "NestedLoops_Simple.cfa", simpleConfig },
        { "LoopMultipleExits_Simple", "LoopMultipleExits.ll", "LoopMultipleExits_Simple.cfa", simpleConfig },
        { "PostTestLoop_Simple", "PostTestLoop.ll", "PostTestLoop_Simple.cfa", simpleConfig },
    };

}

INSTANTIATE_TEST_SUITE_P(
    ModuleToCfaRegressionTests, ModuleToCfaTest,
    ::testing::ValuesIn(GetTestModels()),
    [](const testing::TestParamInfo<ModuleToCfaTestModel>& info) {
        return info.param.mName;
    }
);

} // end anonymous namespace