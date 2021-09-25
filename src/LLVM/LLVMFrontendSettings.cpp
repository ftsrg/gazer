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
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    cl::OptionCategory LLVMFrontendCategory("LLVM frontend settings");
    cl::OptionCategory IrToCfaCategory("LLVM IR translation settings");
    cl::OptionCategory TraceCategory("Traceability settings");
    cl::OptionCategory ChecksCategory("Check instrumentation settings");
} // end namespace gazer

namespace
{
    cl::opt<std::string> EnabledChecks("checks", cl::desc("List of enabled checks"), cl::cat(ChecksCategory));

    // LLVM frontend and transformation options
    // LLVM IR to CFA translation options
    cl::opt<InlineLevel> InlineLevelOpt("inline", cl::desc("Level for variable elimination:"),
        cl::values(
            clEnumValN(InlineLevel::Off, "off", "Do not eliminate variables"),
            clEnumValN(InlineLevel::Default, "default", "Eliminate variables having only one use"),
            clEnumValN(InlineLevel::All, "all", "Eliminate all eligible variables")
        ),
        cl::init(InlineLevel::Default),
        cl::cat(LLVMFrontendCategory)
    );
    cl::opt<bool> NoInlineGlobals(
        "no-inline-globals", cl::desc("Do not inline eligible global variables"), cl::cat(LLVMFrontendCategory));
    cl::opt<bool> NoOptimize(
        "no-optimize", cl::desc("Do not run optimization passes"), cl::cat(LLVMFrontendCategory));
    cl::opt<bool> NoAssertLift(
        "no-assert-lift", cl::desc("Do not lift assertions into the main procedure"), cl::cat(LLVMFrontendCategory)
    );
    cl::opt<bool> NoSlice(
        "no-slicing", cl::desc("Do not run program slicing pass"), cl::cat(LLVMFrontendCategory)
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
    cl::opt<std::string> EntryFunctionName(
        "function", cl::desc("Main function name"), cl::cat(IrToCfaCategory), cl::init("main"));
    cl::opt<bool> Strict(
        "strict", cl::desc("Use stricter transformation rules for undefined behavior"), cl::cat(IrToCfaCategory)
    );

    // Memory models
    cl::opt<bool> DebugDumpMemorySSA(
        "dump-memssa", cl::desc("Dump the built MemorySSA information to stderr"),
        cl::cat(IrToCfaCategory)
    );
    cl::opt<MemoryModelSetting> MemoryModelOpt("memory", cl::desc("Memory model to use:"),
        cl::values(
            clEnumValN(MemoryModelSetting::Flat, "flat", "Bit-precise flat memory model"),
            clEnumValN(MemoryModelSetting::Havoc, "havoc", "Dummy havoc model")
        ),
        cl::init(MemoryModelSetting::Flat),
        cl::cat(IrToCfaCategory)
    );

    // Traceability options
    cl::opt<bool> PrintTrace(
        "trace",
        cl::desc("Print counterexample trace"),
        cl::cat(TraceCategory)
    );
    
    cl::opt<std::string> GenerateWitness(
        "witness",
        cl::desc("Generate an SV-Comp witness file. The name of the output file and usage of flag --hash is required."),
        cl::value_desc("filename"),
        cl::cat(TraceCategory)
    );

    cl::opt<std::string> Hash(
        "hash",
        cl::desc("SHA256 hash of the source program. Required for generating a witness."),
        cl::value_desc("hash"),
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

bool LLVMFrontendSettings::validate(const llvm::Module& llvmModule, llvm::raw_ostream& os) const
{
    if (llvmModule.getFunction(this->function) == nullptr) {
        os << "The entry function '" << this->function << "' does not exist!\n";
        return false;
    }

    return true;
}

llvm::Function* LLVMFrontendSettings::getEntryFunction(const llvm::Module& llvmModule) const
{
    llvm::Function* result = llvmModule.getFunction(this->function);
    assert(result != nullptr && "The entry function must exist!");

    return result;
}

LLVMFrontendSettings LLVMFrontendSettings::initFromCommandLine()
{
    LLVMFrontendSettings settings;

    // opt-out settings
    settings.inlineGlobals = !NoInlineGlobals;
    settings.optimize = !NoOptimize;
    settings.liftAsserts = !NoAssertLift;
    settings.slicing =!NoSlice;
    settings.simplifyExpr = !NoSimplifyExpr;

    settings.strict = Strict;

    settings.inlineLevel = InlineLevelOpt;
    settings.elimVars = ElimVarsLevelOpt;
    settings.memoryModel = MemoryModelOpt;

    settings.checks = EnabledChecks.getValue();


    settings.function = EntryFunctionName.getValue();

    if (ArithInts) {
        settings.ints = IntRepresentation::Integers;
    } else {
        settings.ints = IntRepresentation::BitVectors;
    }

    settings.debugDumpMemorySSA = DebugDumpMemorySSA;

    settings.trace = PrintTrace;
    settings.witness = GenerateWitness.getValue();
    settings.hash = Hash.getValue();
    settings.testHarnessFile = TestHarnessFile.getValue();

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
        default:
            break;
    }
    str += R"(", "loop_representation": ")";

    switch (loops) {
        case LoopRepresentation::Recursion:  str += "recursion"; break;
        case LoopRepresentation::Cycle:      str += "cycle"; break;
    default:
        break;
    }

    str += R"(", "int_representation": ")";

    switch (ints) {
        case IntRepresentation::BitVectors:     str += "bv"; break;
        case IntRepresentation::Integers:       str += "int"; break;
        default:
            break;
    }

    str += R"(", "float_representation": ")";

    switch (floats) {
        case FloatRepresentation::Fpa:     str += "fpa";   break;
        case FloatRepresentation::Real:    str += "real";  break;
        case FloatRepresentation::Undef:   str += "undef"; break;
        default:
            break;
    }

    str += "\"}";

    return str;
}
