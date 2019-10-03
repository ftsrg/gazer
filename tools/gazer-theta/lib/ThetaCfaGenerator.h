#ifndef GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H
#define GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Automaton/CallGraph.h"

#include <llvm/Support/raw_ostream.h>

namespace gazer::theta
{

std::string printThetaExpr(const ExprPtr& expr);

class ThetaCfaGenerator
{
public:
    ThetaCfaGenerator(AutomataSystem& system)
        : mSystem(system), mCallGraph(system)
    {}

    void write(llvm::raw_ostream& os);

private:
    std::string validName(llvm::Twine basename, std::function<bool(llvm::StringRef)> isValid);

private:
    AutomataSystem& mSystem;
    CallGraph mCallGraph;
    unsigned mTmpCount = 0;
    unsigned mLocCount = 0;
};


}

#endif