#ifndef GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H
#define GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Automaton/CallGraph.h"

#include <llvm/Support/raw_ostream.h>

namespace gazer::theta
{

std::string printThetaExpr(const ExprPtr& expr);

std::string printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames);

class ThetaCfaGenerator
{
public:
    ThetaCfaGenerator(AutomataSystem& system)
        : mSystem(system), mCallGraph(system)
    {}

    void write(llvm::raw_ostream& os);

private:
    std::string validName(std::string name, std::function<bool(const std::string&)> isUnique);

private:
    AutomataSystem& mSystem;
    CallGraph mCallGraph;
    unsigned mTmpCount = 0;
    unsigned mLocCount = 0;
};

}

#endif