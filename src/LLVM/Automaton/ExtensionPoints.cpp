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
        name = ("_" + llvm::Twine(mTmp++)).toStringRef(buffer);
    }

    GazerContext& context = mSystem.getContext();
    while (context.getVariable(name) != nullptr) {
        name = (base + llvm::Twine(mTmp++)).toStringRef(buffer);
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

GazerContext& ExtensionPoint::getContext() const
{
    return mGenInfo.Automaton->getParent().getContext();
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
    if (llvm::Function* fun = getSourceFunction()) {
        return fun;
    }

    if (llvm::Loop* loop = getSourceLoop()) {
        return loop->getHeader()->getParent();
    }

    llvm_unreachable("Invalid automaton source!");
}

bool ExtensionPoint::isEntryProcedure() const
{
    llvm::Function* function = getParent();
    llvm::Module* module = function->getParent();
    return mGenInfo.Context.getSettings().getEntryFunction(*module) == function;
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

void LoopVarDeclExtensionPoint::createLoopOutput(ValueOrMemoryObject val, Variable* output, const llvm::Twine& suffix)
{
    std::string name = (val.getName() + suffix).str();
    auto* copyOfVar = mGenInfo.Automaton->createLocal(name, output->getType());

    this->markOutput(val, copyOfVar);
    mGenInfo.LoopOutputs[val] = VariableAssignment(copyOfVar, output->getRefExpr());
}

// Generation step extension point
//===----------------------------------------------------------------------===//

auto GenerationStepExtensionPoint::createAuxiliaryVariable(const std::string& name, Type& type)
    -> Variable*
{
    return mGenInfo.Automaton->createLocal(name, type);
}

auto BlocksToCfa::ExtensionPointImpl::getAsOperand(ValueOrMemoryObject val, Type* type) -> ExprPtr
{
    if (type == nullptr) {
        return mBlocksToCfa.operand(val);
    }

    return mBlocksToCfa.operand(val, *type);
}

bool BlocksToCfa::ExtensionPointImpl::tryToEliminate(ValueOrMemoryObject val, Variable* variable, const ExprPtr& expr)
{
    return mBlocksToCfa.tryToEliminate(val, variable, expr);
}

void BlocksToCfa::ExtensionPointImpl::insertAssignment(Variable* variable, const ExprPtr& value)
{
    mAssigns.emplace_back(variable, value);
}

auto BlocksToCfa::createExtensionPoint(
    std::vector<VariableAssignment>& assignments, Location** entry, Location** exit)
    -> ExtensionPointImpl
{
    return ExtensionPointImpl(*this, assignments, entry, exit);
}

void BlocksToCfa::ExtensionPointImpl::splitCurrentTransition(const ExprPtr& guard)
{
    Location* aux = mGenInfo.Automaton->createLocation();

    mGenInfo.Automaton->createAssignTransition(*mEntry, aux, mBlocksToCfa.mExprBuilder.True(), mAssigns);

    Location* aux2 = mGenInfo.Automaton->createLocation();

    mGenInfo.Automaton->createAssignTransition(aux, aux2, guard, {});
    mAssigns.clear();

    *mEntry = aux2;
}
