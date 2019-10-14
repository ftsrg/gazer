#include "ThetaCfaGenerator.h"

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/Pass.h>

using namespace gazer;

namespace
{

struct ThetaCfaWriterPass : public llvm::ModulePass
{
    static char ID;
    llvm::raw_ostream& mOS;

    ThetaCfaWriterPass(llvm::raw_ostream& os)
        : ModulePass(ID), mOS(os)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<ModuleToAutomataPass>();
        au.setPreservesAll();
    }

    bool runOnModule(llvm::Module& module) override
    {
        auto& system = getAnalysis<ModuleToAutomataPass>().getSystem();

        theta::ThetaNameMapping mapping;
        theta::ThetaCfaGenerator generator{system};

        generator.write(mOS, mapping);

        return false;
    }


};

} // end anonymous namespace

char ThetaCfaWriterPass::ID;

llvm::Pass* gazer::theta::createThetaCfaWriterPass(llvm::raw_ostream& os)
{
    return new ThetaCfaWriterPass(os);
}