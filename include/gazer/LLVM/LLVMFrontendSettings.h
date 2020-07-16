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
#ifndef GAZER_LLVM_LLVMFRONTENDSETTINGS_H
#define GAZER_LLVM_LLVMFRONTENDSETTINGS_H

#include <string>

namespace llvm
{

class raw_ostream;
class Module;
class Function;

} // namespace llvm

namespace gazer
{

enum class InlineLevel
{
    Off,        ///< Do not inline procedures
    Default,    ///< Inline non-recursive, used-only-once procedures
    All         ///< Inline all non-recursive procedures
};

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

enum class MemoryModelSetting
{
    Havoc,
    Flat
};

class LLVMFrontendSettings
{
public:
    LLVMFrontendSettings() = default;

    LLVMFrontendSettings(const LLVMFrontendSettings&) = delete;
    LLVMFrontendSettings& operator=(const LLVMFrontendSettings&) = delete;

    LLVMFrontendSettings(LLVMFrontendSettings&&) = default;
    LLVMFrontendSettings& operator=(LLVMFrontendSettings&&) = default;

public:
    // Traceability
    bool trace = false;
    std::string testHarnessFile;

    // LLVM transformations
    InlineLevel inlineLevel = InlineLevel::Default;
    bool inlineGlobals = false;
    bool optimize = true;
    bool liftAsserts = true;
    bool slicing = true;

    // Checks
    std::string checks = "";

    // IR translation
    ElimVarsLevel elimVars = ElimVarsLevel::Off;
    LoopRepresentation loops = LoopRepresentation::Recursion;
    IntRepresentation ints = IntRepresentation::BitVectors;
    FloatRepresentation floats = FloatRepresentation::Fpa;
    bool simplifyExpr = true;
    bool strict = false;

    std::string function = "main";

    // Memory models
    bool debugDumpMemorySSA = false;
    MemoryModelSetting memoryModel = MemoryModelSetting::Flat;

public:
    /// Returns true if the current settings can be applied to the given module.
    bool validate(const llvm::Module& module, llvm::raw_ostream& os) const;

    llvm::Function* getEntryFunction(const llvm::Module& module) const;

    bool isElimVarsOff() const { return elimVars == ElimVarsLevel::Off; }
    bool isElimVarsNormal() const { return elimVars == ElimVarsLevel::Normal; }
    bool isElimVarsAggressive() const { return elimVars == ElimVarsLevel::Aggressive; }

public:
    static LLVMFrontendSettings initFromCommandLine();
    std::string toString() const;
};

} // end namespace gazer

#endif
