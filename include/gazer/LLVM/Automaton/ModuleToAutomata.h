#ifndef GAZER_MODULETOAUTOMATA_H
#define GAZER_MODULETOAUTOMATA_H

#include "gazer/Automaton/Cfa.h"

#include <llvm/Pass.h>

namespace llvm {
    class LoopInfo;
}

namespace gazer
{

class ModuleToAutomataPass : public llvm::ModulePass
{
public:
    static char ID;

    ModuleToAutomataPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Module to automata transformation";
    }
};

std::unique_ptr<AutomataSystem> translateModuleToAutomata(
    llvm::Module& module,
    std::unordered_map<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context
);

}


#endif //GAZER_MODULETOAUTOMATA_H
