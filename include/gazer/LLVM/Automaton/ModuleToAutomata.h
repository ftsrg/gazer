#ifndef GAZER_MODULETOAUTOMATA_H
#define GAZER_MODULETOAUTOMATA_H

#include "gazer/Automaton/Cfa.h"

#include <llvm/Pass.h>

namespace llvm
{
    class LoopInfo;
    class Value;
}

namespace gazer
{

class MemoryModel;

class ModuleToAutomataSettings
{
public:
    enum ElimVarsLevel
    {
        ElimVars_Off,       ///< Do not try to eliminate variables
        ElimVars_Normal,    ///< Inline variables which have only one use
        ElimVars_Aggressive ///< Inline all suitable variables
    };

    enum LoopRepresentation
    {
        Loops_Recursion,    ///< Represents loops as recursive functions.
        Loops_Cycle,        ///< Represents loops as cycles.
    };

    enum IntRepresentation
    {
        Ints_UseBv,         ///< Use bitvectors to represent integer types.
        Ints_UseInt,        ///< Use the arithmetic integer type to represent integers.
    };

    enum FloatRepresentation
    {
        Floats_UseFpa,      ///< Use floating-point bitvectors to represent floats.
        Floats_UseReal,     ///< Approximate floats using rational types.
        Floats_UseUndef     ///< Use undef's for float operations.
    };

public:
    ModuleToAutomataSettings(
        ElimVarsLevel elimVars = ElimVars_Off,
        LoopRepresentation loops = Loops_Recursion,
        IntRepresentation ints = Ints_UseBv,
        FloatRepresentation floats = Floats_UseFpa
    )
        : mElimVars(elimVars), mLoops(loops), mInts(ints), mFloats(floats)
    {}

    ElimVarsLevel       getElimVarsLevel()       const { return mElimVars; }
    LoopRepresentation  getLoopRepresentation()  const { return mLoops; }
    IntRepresentation   getIntRepresentation()   const { return mInts; }
    FloatRepresentation getFloatRepresentation() const { return mFloats; }

    void setElimVarsLevel(ElimVarsLevel value) { mElimVars = value; }
    void setLoopRepresentation(LoopRepresentation value) { mLoops = value; }
    void setIntRepresentation(IntRepresentation value) { mInts = value; }
    void setFloatRepresentation(FloatRepresentation value) { mFloats = value; }

    bool isElimVarsOff() const { return mElimVars == ElimVars_Off; }
    bool isElimVarsNormal() const { return mElimVars == ElimVars_Normal; }
    bool isElimVarsAggressive() const { return mElimVars == ElimVars_Aggressive; }


private:
    ElimVarsLevel mElimVars;
    LoopRepresentation mLoops;
    IntRepresentation mInts;
    FloatRepresentation mFloats;
};


class ModuleToAutomataPass : public llvm::ModulePass
{
public:
    static char ID;

    ModuleToAutomataPass(GazerContext& context)
        : ModulePass(ID), mContext(context)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Module to automata transformation";
    }

    AutomataSystem& getSystem() { return *mSystem; }
    llvm::DenseMap<llvm::Value*, Variable*>& getVariableMap() {
        return mVariables;
    }

private:
    std::unique_ptr<AutomataSystem> mSystem;
    llvm::DenseMap<llvm::Value*, Variable*> mVariables;
    llvm::DenseMap<Location*, llvm::BasicBlock*> mBlocks;
    GazerContext& mContext;
};

std::unique_ptr<AutomataSystem> translateModuleToAutomata(
    llvm::Module& module,
    ModuleToAutomataSettings settings,
    std::unordered_map<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
);

}

#endif //GAZER_MODULETOAUTOMATA_H
