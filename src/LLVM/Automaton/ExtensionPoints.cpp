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

#include "FunctionToCfa.h"

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

using namespace gazer;
using namespace gazer::llvm2cfa;

// CfaGenInfo and GenerationContext implementations
//===----------------------------------------------------------------------===//
// TODO: Maybe these should moved into another source file?

std::string GenerationContext::uniqueName(const llvm::Twine& base)
{
    llvm::SmallString<64> buffer;
    llvm::StringRef name = base.toStringRef(buffer);

    if (name.empty()) {
        llvm::Twine tempName = "_" + llvm::Twine(mTmp++);
        name = tempName.toStringRef(buffer);
    }

    GazerContext& context = mSystem.getContext();
    while (context.getVariable(name) != nullptr) {
        llvm::Twine newName = base + llvm::Twine(mTmp++);
        name = newName.toStringRef(buffer);
    }

    return buffer.str();
}

void CfaGenInfo::addVariableToContext(ValueOrMemoryObject value, Variable* variable)
{
    Context.addExprValueIfTraceEnabled(Automaton, value, variable->getRefExpr());
}

void CfaGenInfo::addBlockToLocationsMapping(const llvm::BasicBlock* bb, Location* entry, Location* exit)
{
    Blocks[bb] = std::make_pair(entry, exit);
    Context.addReverseBlockIfTraceEnabled(bb, entry, CfaToLLVMTrace::Location_Entry);
    Context.addReverseBlockIfTraceEnabled(bb, exit, CfaToLLVMTrace::Location_Exit);
}

// Extension point implementations
//===----------------------------------------------------------------------===//

const Cfa& ExtensionPoint::getCfa() const
{
    return *mGenInfo.Automaton;
}

llvm::Loop* ExtensionPoint::getSourceLoop() const
{
    return mGenInfo.getSourceLoop();
}

llvm::Function* ExtensionPoint::getSourceFunction() const
{
    return mGenInfo.getSourceFunction();
}

llvm::Function* ExtensionPoint::getParent() const
{
    if (auto fun = getSourceFunction()) {
        return fun;
    }

    if (auto loop = getSourceLoop()) {
        return loop->getHeader()->getParent();
    }

    llvm_unreachable("Invalid automaton source!");
}

Variable* VariableDeclExtensionPoint::createInput(ValueOrMemoryObject val, Type& type, const std::string& suffix)
{
    Cfa* cfa = mGenInfo.Automaton;

    auto name = val.hasName() ? val.getName() + suffix : "_" + suffix;
    Variable* variable = cfa->createInput(name, type);
    mGenInfo.addInput(val, variable);

    return variable;
}

Variable* VariableDeclExtensionPoint::createLocal(ValueOrMemoryObject val, Type& type, const std::string& suffix)
{
    Cfa* cfa = mGenInfo.Automaton;

    auto name = val.hasName() ? val.getName() + suffix : "_" + suffix;
    Variable* variable = cfa->createLocal(name, type);
    mGenInfo.addLocal(val, variable);

    return variable;
}

Variable* VariableDeclExtensionPoint::createPhiInput(ValueOrMemoryObject val, Type& type, const std::string& suffix)
{
    Cfa* cfa = mGenInfo.Automaton;

    std::string name = val.hasName() ? val.getName() + suffix : "_" + suffix;
    Variable* variable = cfa->createInput(name, type);
    mGenInfo.addPhiInput(val, variable);

    return variable;
}

void VariableDeclExtensionPoint::markOutput(ValueOrMemoryObject val, Variable* variable)
{
    mGenInfo.Automaton->addOutput(variable);
    mGenInfo.Outputs[val] = variable;
}

Variable* AutomatonInterfaceExtensionPoint::getInputVariableFor(ValueOrMemoryObject val)
{
    return mGenInfo.findInput(val);
}

Variable* AutomatonInterfaceExtensionPoint::getOutputVariableFor(ValueOrMemoryObject val)
{
    return mGenInfo.findOutput(val);
}

Variable* AutomatonInterfaceExtensionPoint::getVariableFor(ValueOrMemoryObject val)
{
    return mGenInfo.findVariable(val);
}

Variable* GenerationStepExtensionPoint::createAuxiliaryVariable(const std::string& name, Type& type)
{
    return mGenInfo.Automaton->createLocal(name, type);
}