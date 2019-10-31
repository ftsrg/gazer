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
    Cycle       ///< Represent loops as cycles.
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
    // Traceability
    bool trace = false;
    std::string testHarnessFile;

    // LLVM transformations
    bool inlineFunctions = false;
    bool inlineGlobals = false;
    bool optimize = true;
    bool liftAsserts = true;

    // IR translation
    ElimVarsLevel elimVars = ElimVarsLevel::Off;
    LoopRepresentation loops = LoopRepresentation::Recursion;
    IntRepresentation ints = IntRepresentation::BitVectors;
    FloatRepresentation floats = FloatRepresentation::Fpa;
    bool simplifyExpr = true;

public:
    bool isElimVarsOff() const { return elimVars == ElimVarsLevel::Off; }
    bool isElimVarsNormal() const { return elimVars == ElimVarsLevel::Normal; }
    bool isElimVarsAggressive() const { return elimVars == ElimVarsLevel::Aggressive; }

public:
    static LLVMFrontendSettings initFromCommandLine();
    std::string toString() const;
};

} // end namespace gazer

#endif
