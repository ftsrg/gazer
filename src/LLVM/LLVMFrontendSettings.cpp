//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/LLVMFrontendSettings.h"

#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    cl::OptionCategory LLVMFrontendCategory("LLVM frontend settings");
    cl::OptionCategory IrToCfaCategory("LLVM IR translation settings");
    cl::OptionCategory TraceCategory("Traceability settings");
} // end namespace gazer

namespace
{
    // LLVM frontend and transformation options
    cl::opt<bool> InlineFunctions(
        "inline", cl::desc("Inline function calls"), cl::cat(LLVMFrontendCategory));
    cl::opt<bool> InlineGlobals(
        "inline-globals", cl::desc("Inline global variables"), cl::cat(LLVMFrontendCategory));
    cl::opt<bool> NoOptimize(
        "no-optimize", cl::desc("Do not run optimization passes"), cl::cat(LLVMFrontendCategory));
    cl::opt<bool> NoAssertLift(
        "no-assert-lift", cl::desc("Do not lift assertions into the main procedure"), cl::cat(LLVMFrontendCategory)
    );

    // LLVM IR to CFA translation options
    cl::opt<ElimVarsLevel> ElimVarsLevelOpt("elim-vars", cl::desc("Level for variable elimination:"),
        cl::values(
            clEnumValN(ElimVarsLevel::Off, "off", "Do not eliminate variables"),
            clEnumValN(ElimVarsLevel::Normal, "normal", "Eliminate variables having only one use"),
            clEnumValN(ElimVarsLevel::Aggressive, "aggressive", "Eliminate all eligible variables")
        ),
        cl::init(ElimVarsLevel::Normal),
        cl::cat(IrToCfaCategory)
    );
    cl::opt<bool> ArithInts(
        "math-int", cl::desc("Use mathematical unbounded integers instead of bitvectors"),
        cl::cat(IrToCfaCategory));
    cl::opt<bool> NoSimplifyExpr(
        "no-simplify-expr", cl::desc("Do not simplify expressions"),
        cl::cat(IrToCfaCategory)
    );

    // Memory models
    cl::opt<bool> DebugDumpMemorySSA(
        "dump-memssa", cl::desc("Dump the built MemorySSA information to stderr"),
        cl::cat(IrToCfaCategory)
    );

    // Traceability options
    cl::opt<bool> PrintTrace(
        "trace",
        cl::desc("Print counterexample trace"),
        cl::cat(TraceCategory)
    );
    cl::opt<std::string> TestHarnessFile(
        "test-harness",
        cl::desc("Write test harness to output file"),
        cl::value_desc("filename"),
        cl::init(""),
        cl::cat(TraceCategory)
    );
} // end anonymous namespace

LLVMFrontendSettings LLVMFrontendSettings::initFromCommandLine()
{
    LLVMFrontendSettings settings;

    settings.inlineFunctions = InlineFunctions;
    settings.inlineGlobals = InlineGlobals;
    settings.optimize = !NoOptimize;
    settings.liftAsserts = !NoAssertLift;
    settings.elimVars = ElimVarsLevelOpt;
    settings.simplifyExpr = !NoSimplifyExpr;

    if (ArithInts) {
        settings.ints = IntRepresentation::Integers;
    } else {
        settings.ints = IntRepresentation::BitVectors;
    }

    settings.debugDumpMemorySSA = DebugDumpMemorySSA;

    settings.trace = PrintTrace;
    settings.testHarnessFile = TestHarnessFile;

    return settings;
}

std::string LLVMFrontendSettings::toString() const
{
    std::string str;

    str += R"({"elim_vars": ")";
    switch (elimVars) {
        case ElimVarsLevel::Off:         str += "off"; break;
        case ElimVarsLevel::Normal:      str += "normal"; break;
        case ElimVarsLevel::Aggressive:  str += "aggressive"; break;
    }
    str += R"(", "loop_representation": ")";

    switch (loops) {
        case LoopRepresentation::Recursion:  str += "recursion"; break;
        case LoopRepresentation::Cycle:      str += "cycle"; break;
    }

    str += R"(", "int_representation": ")";

    switch (ints) {
        case IntRepresentation::BitVectors:     str += "bv"; break;
        case IntRepresentation::Integers:       str += "int"; break;
    }

    str += R"(", "float_representation": ")";

    switch (floats) {
        case FloatRepresentation::Fpa:     str += "fpa";   break;
        case FloatRepresentation::Real:    str += "real";  break;
        case FloatRepresentation::Undef:   str += "undef"; break;
    }

    str += "\"}";

    return str;
}
