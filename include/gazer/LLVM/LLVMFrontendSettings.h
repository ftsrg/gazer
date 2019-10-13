#ifndef GAZER_LLVM_LLVMFRONTENDSETTINGS_H
#define GAZER_LLVM_LLVMFRONTENDSETTINGS_H

#include <string>

namespace gazer
{

enum class IntRepresentation
{
    BitVectors, ///< Use bitvectors to represent integer types.
    Integers    ///< Use the arithmetic integer type to represent integers.
};

enum class FloatRepresentation
{
    Fpa,        ///< Use floating-point bitvectors to represent floats.
    Real,       ///< Approximate floats using rational types.
    Undef       ///< Use undef's for float operations.
};

enum class LoopRepresentation
{
    Recursion,  ///< Represent loops as recursive functions.
    Cycle,      ///< Represent loops as cycles.
};

enum class ElimVarsLevel
{
    Off,       ///< Do not try to eliminate variables
    Normal,    ///< Inline variables which have only one use
    Aggressive ///< Inline all suitable variables
};

class LLVMFrontendSettings
{
public:
    explicit LLVMFrontendSettings(
        ElimVarsLevel elimVars = ElimVarsLevel::Off,
        LoopRepresentation loops = LoopRepresentation::Recursion,
        IntRepresentation ints = IntRepresentation::BitVectors,
        FloatRepresentation floats = FloatRepresentation::Fpa,
        bool simplifyExpr = true,
        bool trace = true
    )
        : mElimVars(elimVars), mLoops(loops), mInts(ints), mFloats(floats),
        mSimplifyExpr(simplifyExpr), mTrace(trace)
    {}

    static LLVMFrontendSettings initFromCommandLine();

    ElimVarsLevel       getElimVarsLevel()       const { return mElimVars; }
    LoopRepresentation  getLoopRepresentation()  const { return mLoops; }
    IntRepresentation   getIntRepresentation()   const { return mInts; }
    FloatRepresentation getFloatRepresentation() const { return mFloats; }

    void setElimVarsLevel(ElimVarsLevel value) { mElimVars = value; }
    void setLoopRepresentation(LoopRepresentation value) { mLoops = value; }
    void setIntRepresentation(IntRepresentation value) { mInts = value; }
    void setFloatRepresentation(FloatRepresentation value) { mFloats = value; }
    void setSimplifyExpr(bool value) { mSimplifyExpr = value; }
    void setTrace(bool trace) { mTrace = trace; }

    bool isElimVarsOff() const { return mElimVars == ElimVarsLevel::Off; }
    bool isElimVarsNormal() const { return mElimVars == ElimVarsLevel::Normal; }
    bool isElimVarsAggressive() const { return mElimVars == ElimVarsLevel::Aggressive; }
    bool isSimplifyExpr() const { return mSimplifyExpr; }
    bool isTraceEnabled() const { return mTrace; }

    std::string toString() const;

private:
    ElimVarsLevel mElimVars;
    LoopRepresentation mLoops;
    IntRepresentation mInts;
    FloatRepresentation mFloats;
    bool mSimplifyExpr;
    bool mTrace;
};

} // end namespace gazer

#endif
