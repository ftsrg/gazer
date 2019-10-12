#ifndef GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H
#define GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Automaton/CallGraph.h"

#include <llvm/Support/raw_ostream.h>

namespace gazer::theta
{

std::string printThetaExpr(const ExprPtr& expr);

std::string printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames);

struct ThetaNameMapping
{
    llvm::StringMap<Location*> locations;
    llvm::StringMap<Variable*> variables;
    Location* errorLocation;
    Variable* errorFieldVariable;
    llvm::DenseMap<Location*, Location*> inlinedLocations;
    llvm::DenseMap<Variable*, Variable*> inlinedVariables;
};

class ThetaCfaGenerator
{
public:
    ThetaCfaGenerator(AutomataSystem& system)
        : mSystem(system), mCallGraph(system)
    {}

    void write(llvm::raw_ostream& os, ThetaNameMapping& names);

private:
    std::string validName(std::string name, std::function<bool(const std::string&)> isUnique);

private:
    AutomataSystem& mSystem;
    CallGraph mCallGraph;
    unsigned mTmpCount = 0;
};

}

#endif