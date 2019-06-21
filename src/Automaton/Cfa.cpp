#include <gazer/Core/LiteralExpr.h>
#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/DepthFirstIterator.h>

#include <boost/iterator/indirect_iterator.hpp>

using namespace gazer;

Cfa::Cfa(GazerContext &context, std::string name, AutomataSystem* parent)
    : mName(name), mContext(context)
{
    // Create an entry and exit location
    Location* entry = this->createLocation();
    Location* exit = this->createLocation();

    mEntry = entry;
    mExit = exit;
}

// Location and edge construction
//-------------------------------------------------------------------------
Location* Cfa::createLocation()
{
    Location* loc = new Location(mLocationIdx++);
    mLocations.emplace_back(loc);

    return loc;
}

Location* Cfa::createErrorLocation()
{
    Location* loc = new Location(mLocationIdx++, Location::Error);
    mLocations.emplace_back(loc);
    mErrorLocations.emplace_back(loc);

    return loc;
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, ExprPtr guard,
        std::vector<VariableAssignment> assignments)
{
    auto edge = new AssignTransition(source, target, guard, assignments);
    mTransitions.emplace_back(edge);
    source->addOutgoing(edge);
    target->addIncoming(edge);

    return edge;
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target) {
    return createAssignTransition(source, target, BoolLiteralExpr::True(mContext), {});
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, std::vector<VariableAssignment> assignments)
{
    return createAssignTransition(source, target, BoolLiteralExpr::True(mContext), assignments);
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, ExprPtr guard)
{
    return createAssignTransition(source, target, guard, {});
}

CallTransition *Cfa::createCallTransition(Location *source, Location *target, ExprPtr guard,
    Cfa *callee, std::vector<ExprPtr> inputArgs, std::vector<VariableAssignment> outputArgs)
{
    assert(source != nullptr);
    assert(target != nullptr);

    auto call = new CallTransition(source, target, guard, callee, inputArgs, outputArgs);
    mTransitions.emplace_back(call);
    source->addOutgoing(call);
    target->addIncoming(call);

    return call;
}

CallTransition *Cfa::createCallTransition(
    Location *source,
    Location *target,
    Cfa *callee,
    std::vector<ExprPtr> inputArgs,
    std::vector<VariableAssignment> outputArgs)
{
    return createCallTransition(source, target, BoolLiteralExpr::True(mContext), callee, inputArgs, outputArgs);
}

// Member variables
//-----------------------------------------------------------------------------

Variable *Cfa::createInput(llvm::StringRef name, Type& type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mInputs.push_back(variable);

    return variable;
}

void Cfa::addOutput(Variable* variable)
{
    mOutputs.push_back(variable);
}

Variable *Cfa::createLocal(llvm::StringRef name, Type& type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mLocals.push_back(variable);

    return variable;
}

Variable *Cfa::createMemberVariable(llvm::Twine name, Type &type)
{
    llvm::Twine variableName = llvm::Twine(mName, "/") + name;

    return mContext.createVariable(variableName.str(), type);
}

void Cfa::addNestedAutomaton(Cfa* cfa)
{
    assert(cfa != nullptr);
    assert(cfa->mParentAutomaton == nullptr);

    mNestedAutomata.push_back(cfa);
    cfa->mParentAutomaton = this;
}

size_t Cfa::getInputNumber(gazer::Variable* variable) const
{
    auto it = std::find(mInputs.begin(), mInputs.end(), variable);
    assert(it != mInputs.end() && "Variable must be present in the input list!");

    return std::distance(mInputs.begin(), it);
}

size_t Cfa::getOutputNumber(gazer::Variable* variable) const
{
    auto it = std::find(mOutputs.begin(), mOutputs.end(), variable);
    assert(it != mOutputs.end() && "Variable must be present in the output list!");

    return std::distance(mOutputs.begin(), it);
}

bool Cfa::isOutput(Variable* variable) const
{
    return std::find(mOutputs.begin(), mOutputs.end(), variable) != mOutputs.end();
}

Variable* Cfa::findVariableByName(const std::vector<Variable*>& vec, llvm::StringRef name) const
{
    auto variableName = llvm::Twine(mName, "/") + name;
    Variable* variable = mContext.getVariable(variableName.str());
    if (variable == nullptr) {
        return nullptr;
    }

    // We must also make sure that the required vector contains said variable.
    if (std::find(vec.begin(), vec.end(), variable) == vec.end()) {
        return nullptr;
    }

    return variable;
}

Variable* Cfa::findInputByName(llvm::StringRef name) const
{
    return findVariableByName(mInputs, name);
}

Variable* Cfa::findLocalByName(llvm::StringRef name) const
{
    return findVariableByName(mLocals, name);
}

Variable* Cfa::findOutputByName(llvm::StringRef name) const
{
    return findVariableByName(mOutputs, name);
}

// Transformations
//-----------------------------------------------------------------------------

void Cfa::disconnectLocation(Location* location)
{
    for (Transition* edge : location->incoming()) {
        edge->mTarget = nullptr;
        edge->mSource->removeOutgoing(edge);
    }

    for (Transition* edge : location->outgoing()) {
        edge->mSource = nullptr;
        edge->mTarget->removeIncoming(edge);
    }

    location->mIncoming.clear();
    location->mOutgoing.clear();
}

void Cfa::disconnectEdge(Transition* edge)
{
    edge->getSource()->removeOutgoing(edge);
    edge->getTarget()->removeIncoming(edge);

    edge->mSource = nullptr;
    edge->mTarget = nullptr;
}

void Cfa::removeUnreachableLocations()
{
    llvm::df_iterator_default_set<Location*> visited;
    auto begin = llvm::df_ext_begin(*this, visited);
    auto end = llvm::df_ext_end(*this, visited);

    for (auto it = begin; it != end; ++it) {
        // The DFS algorithm is executed by running the iterators.
    }

    std::vector<Location*> unreachable;
    for (auto& loc : mLocations) {
        if (visited.count(&*loc) == 0) {
            this->disconnectLocation(&*loc);
        }
    }

    this->clearDisconnectedElements();
}

void Cfa::clearDisconnectedElements()
{
    mLocations.erase(std::remove_if(mLocations.begin(), mLocations.end(), [](auto& loc) {
        return loc->mIncoming.empty() && loc->mOutgoing.empty();
    }), mLocations.end());

    mTransitions.erase(std::remove_if(mTransitions.begin(), mTransitions.end(), [](auto& edge) {
        return edge->mSource == nullptr || edge->mTarget == nullptr;
    }), mTransitions.end());
}


// Support code for locations
//-----------------------------------------------------------------------------

void Location::addIncoming(Transition *edge)
{
    assert(edge->getTarget() == this);
    mIncoming.push_back(edge);
}

void Location::addOutgoing(Transition *edge)
{
    assert(edge->getSource() == this);
    mOutgoing.push_back(edge);
}

void Location::removeIncoming(Transition *edge)
{
    mIncoming.erase(std::remove(mIncoming.begin(), mIncoming.end(), edge), mIncoming.end());
}

void Location::removeOutgoing(Transition *edge)
{
    mOutgoing.erase(std::remove(mOutgoing.begin(), mOutgoing.end(), edge), mOutgoing.end());
}

// Transitions
//-----------------------------------------------------------------------------
AssignTransition::AssignTransition(
    Location *source, Location *target, ExprPtr guard,
    std::vector<VariableAssignment> assignments
) : Transition(source, target, guard, Transition::Edge_Assign), mAssignments(assignments)
{}

CallTransition::CallTransition(
    Location *source, Location *target, ExprPtr guard, Cfa *callee,
    std::vector<ExprPtr> inputArgs, std::vector<VariableAssignment> outputArgs
) : Transition(source, target, guard, Transition::Edge_Call), mCallee(callee),
    mInputArgs(inputArgs), mOutputArgs(outputArgs)
{
    assert(source != nullptr);
    assert(target != nullptr);
    assert(callee != nullptr);
    assert(callee->getNumInputs() == inputArgs.size());
}

// Automata system
//-----------------------------------------------------------------------------

AutomataSystem::AutomataSystem(GazerContext &context)
    : mContext(context)
{}

Cfa *AutomataSystem::createCfa(std::string name)
{
    Cfa* cfa = new Cfa(mContext, name, this);
    mAutomata.emplace_back(cfa);

    return cfa;
}

Cfa* AutomataSystem::createNestedCfa(Cfa* parent, std::string name)
{
    auto fullName = parent->getName() + "/" + name;
    Cfa* cfa = new Cfa(mContext, fullName.str(), this);
    mAutomata.emplace_back(cfa);

    parent->addNestedAutomaton(cfa);

    return cfa;
}

void AutomataSystem::addGlobalVariable(Variable* variable)
{
    mGlobalVariables.push_back(variable);
}

Cfa* AutomataSystem::getAutomatonByName(llvm::StringRef name) const
{
    auto result = std::find_if(begin(), end(), [name](Cfa& cfa) {
        return cfa.getName() == name;
    });

    if (result == end()) {
        return nullptr;
    }

    return &*result;
}