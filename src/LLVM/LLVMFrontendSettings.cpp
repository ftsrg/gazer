#include "gazer/LLVM/LLVMFrontendSettings.h"

#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

namespace
{
} // end anonymous namespace

namespace gazer
{
    // NoSimplifyExpr is a global setting, defined in BoundedModelChecker.cpp
    extern cl::opt<bool> NoSimplifyExpr;
} // end namespace gazer

std::string LLVMFrontendSettings::toString() const
{
    std::string str;

    str += R"({"elim_vars": ")";
    switch (mElimVars) {
        case ElimVarsLevel::Off:         str += "off"; break;
        case ElimVarsLevel::Normal:      str += "normal"; break;
        case ElimVarsLevel::Aggressive:  str += "aggressive"; break;
    }
    str += R"(", "loop_representation": ")";

    switch (mLoops) {
        case LoopRepresentation::Recursion:  str += "recursion"; break;
        case LoopRepresentation::Cycle:      str += "cycle"; break;
    }

    str += R"(", "int_representation": ")";

    switch (mInts) {
        case IntRepresentation::BitVectors:     str += "bv"; break;
        case IntRepresentation::Integers:       str += "int"; break;
    }

    str += R"(", "float_representation": ")";

    switch (mFloats) {
        case FloatRepresentation::Fpa:     str += "fpa";   break;
        case FloatRepresentation::Real:    str += "real";  break;
        case FloatRepresentation::Undef:   str += "undef"; break;
    }

    str += "\"}";

    return str;
}
