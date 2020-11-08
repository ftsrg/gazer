//==- Cfa.h - Control flow automaton interface ------------------*- C++ -*--==//
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
///
/// \file This file defines classes to represent control flow automata, their
/// locations and transitions.
///
//===----------------------------------------------------------------------===//
#ifndef GAZER_AUTOMATON_CFA_H
#define GAZER_AUTOMATON_CFA_H

#include "gazer/Core/Expr.h"
#include "gazer/ADT/Graph.h"

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/DenseMap.h>
#include <boost/iterator/indirect_iterator.hpp>

namespace gazer
{

class Cfa;
class Transition;

class Location : public GraphNode<Location, Transition>
{
    friend class Cfa;
public:
    enum LocationKind
    {
        State,
        Error
    };

private:
    explicit Location(unsigned id, Cfa* parent, LocationKind kind = State)
        : mID(id), mCfa(parent), mKind(kind)
    {}
    
public:
    Location(const Location&) = delete;
    Location& operator=(const Location&) = delete;

    unsigned getId() const { return mID; }
    bool isError() const { return mKind == Error; }

    Cfa* getAutomaton() const { return mCfa; }
private:
    unsigned mID;
    Cfa* mCfa;
    LocationKind mKind;
};

/// A simple transition with a guard or summary expression.
class Transition : public GraphEdge<Location, Transition>
{
    friend class Cfa;
public:
    enum EdgeKind
    {
        Edge_Assign,    ///< Variable assignment.
        Edge_Call,      ///< Call into another procedure.
    };

protected:
    Transition(Location* source, Location* target, ExprPtr expr, EdgeKind kind);
public:
    Transition(Transition&) = delete;
    Transition& operator=(Transition&) = delete;

    ExprPtr getGuard() const { return mExpr; }
    EdgeKind getKind() const { return mEdgeKind; }

    bool isAssign() const { return mEdgeKind == Edge_Assign; }
    bool isCall() const { return mEdgeKind == Edge_Call; }

    void print(llvm::raw_ostream& os) const;

    virtual ~Transition() = default;

private:
    ExprPtr mExpr;
    EdgeKind mEdgeKind;
};

/// Represents a (potentially guared) transition with variable assignments.
class AssignTransition final : public Transition
{
    friend class Cfa;
protected:
    AssignTransition(Location* source, Location* target, ExprPtr guard, std::vector<VariableAssignment> assignments);

public:
    using iterator = std::vector<VariableAssignment>::const_iterator;
    iterator begin() const { return mAssignments.begin(); }
    iterator end() const { return mAssignments.end(); }

    size_t getNumAssignments() const { return mAssignments.size(); }
    void addAssignment(VariableAssignment assignment) {
        mAssignments.push_back(assignment);
    }

    static bool classof(const Transition* edge) {
        return edge->getKind() == Edge_Assign;
    }

private:
    std::vector<VariableAssignment> mAssignments;
};

/// Represents a (potentially guarded) transition with a procedure call.
class CallTransition final : public Transition
{
    friend class Cfa;
protected:
    CallTransition(
        Location* source, Location* target,
        ExprPtr guard,
        Cfa* callee,
        std::vector<VariableAssignment> inputArgs,
        std::vector<VariableAssignment> outputArgs
    );

public:
    Cfa* getCalledAutomaton() const { return mCallee; }

    //-------------------------- Iterator support ---------------------------//
    using input_arg_iterator  = std::vector<VariableAssignment>::const_iterator;
    using output_arg_iterator = std::vector<VariableAssignment>::const_iterator;

    input_arg_iterator input_begin() const { return mInputArgs.begin(); }
    input_arg_iterator input_end() const { return mInputArgs.end(); }
    llvm::iterator_range<input_arg_iterator> inputs() const {
        return llvm::make_range(input_begin(), input_end());
    }
    size_t getNumInputs() const { return mInputArgs.size(); }

    output_arg_iterator output_begin() const { return mOutputArgs.begin(); }
    output_arg_iterator output_end() const { return mOutputArgs.end(); }
    llvm::iterator_range<output_arg_iterator> outputs() const {
        return llvm::make_range(output_begin(), output_end());
    }
    size_t getNumOutputs() const { return mOutputArgs.size(); }

    std::optional<VariableAssignment> getInputArgument(Variable& input) const;
    std::optional<VariableAssignment> getOutputArgument(Variable& variable) const;

    static bool classof(const Transition* edge) {
        return edge->getKind() == Edge_Call;
    }

private:
    Cfa* mCallee;
    std::vector<VariableAssignment> mInputArgs;
    std::vector<VariableAssignment> mOutputArgs;
};

class AutomataSystem;

/// Represents a control flow automaton.
class Cfa final : public Graph<Location, Transition>
{
    friend class AutomataSystem;
private:
    Cfa(GazerContext& context, std::string name, AutomataSystem& parent);

public:
    Cfa(const Cfa&) = delete;
    Cfa& operator=(const Cfa&) = delete;

public:
    //===----------------------------------------------------------------------===//
    // Locations and edges
    Location* createLocation();
    Location* createErrorLocation();

    AssignTransition* createAssignTransition(
        Location* source, Location* target, ExprPtr guard = nullptr, llvm::ArrayRef<VariableAssignment> assignments = {}
    );

    AssignTransition* createAssignTransition(
        Location* source, Location* target, llvm::ArrayRef<VariableAssignment> assignments
    ) {
        return createAssignTransition(source, target, nullptr, assignments);
    }

    CallTransition* createCallTransition(
        Location* source, Location* target, const ExprPtr& guard,
        Cfa* callee, llvm::ArrayRef<VariableAssignment> inputArgs, llvm::ArrayRef<VariableAssignment> outputArgs
    );

    CallTransition* createCallTransition(
        Location* source, Location* target,
        Cfa* callee, llvm::ArrayRef<VariableAssignment> inputArgs, llvm::ArrayRef<VariableAssignment> outputArgs
    );

    //===----------------------------------------------------------------------===//
    // Variable handling

    Variable* createInput(const std::string& name, Type& type);
    Variable* createLocal(const std::string& name, Type& type);

    /// Marks an already existing variable as an output.
    void addOutput(Variable* variable);

    void addErrorCode(Location* location, ExprPtr errorCodeExpr);
    ExprPtr getErrorFieldExpr(Location* location);

    //===----------------------------------------------------------------------===//
    // Iterator for error locations
    using error_iterator = llvm::SmallDenseMap<Location*, ExprPtr, 1>::const_iterator;

    error_iterator error_begin() const { return mErrorFieldExprs.begin(); }
    error_iterator error_end() const { return mErrorFieldExprs.end(); }
    llvm::iterator_range<error_iterator> errors() const {
        return llvm::make_range(error_begin(), error_end());
    }

    size_t getNumErrors() const { return mErrorFieldExprs.size(); }

    // Variable iterators
    using var_iterator = boost::indirect_iterator<std::vector<Variable*>::iterator>;

    var_iterator input_begin() { return mInputs.begin(); }
    var_iterator input_end() { return mInputs.end(); }
    llvm::iterator_range<var_iterator> inputs() {
        return llvm::make_range(input_begin(), input_end());
    }

    var_iterator output_begin() { return mOutputs.begin(); }
    var_iterator output_end() { return mOutputs.end(); }
    llvm::iterator_range<var_iterator> outputs() {
        return llvm::make_range(output_begin(), output_end());
    }

    var_iterator local_begin() { return mLocals.begin(); }
    var_iterator local_end() { return mLocals.end(); }
    llvm::iterator_range<var_iterator> locals() {
        return llvm::make_range(local_begin(), local_end());
    }

    //===----------------------------------------------------------------------===//
    // Others
    llvm::StringRef getName() const { return mName; }
    Location* getEntry() const { return mEntry; }
    Location* getExit() const { return mExit; }

    AutomataSystem& getParent() const { return mParent; }

    size_t getNumLocations() const { return node_size(); }
    size_t getNumTransitions() const { return edge_size(); }

    size_t getNumInputs() const { return mInputs.size(); }
    size_t getNumOutputs() const { return mOutputs.size(); }
    size_t getNumLocals() const { return mLocals.size(); }

    /// Returns the index of a given input variable in the input list of this automaton.

    size_t getInputNumber(Variable* variable) const;
    size_t getOutputNumber(Variable* variable) const;

    Variable* findInputByName(llvm::StringRef name) const;
    Variable* findLocalByName(llvm::StringRef name) const;
    Variable* findOutputByName(llvm::StringRef name) const;

    Variable* getInput(size_t i) const { return mInputs[i]; }
    Variable* getOutput(size_t i) const { return mOutputs[i]; }

    Location* findLocationById(unsigned id);

    bool isOutput(Variable* variable) const;

    /// View the graph representation of this CFA with the
    /// system's default GraphViz viewer.
    void view() const;

    void print(llvm::raw_ostream& os) const;
    void printDeclaration(llvm::raw_ostream& os) const;

    //------------------------------ Deletion -------------------------------//
    void removeUnreachableLocations();

    template<class Predicate>
    void removeLocalsIf(Predicate p) {
        mLocals.erase(
            std::remove_if(mLocals.begin(), mLocals.end(), p),
            mLocals.end()
        );
    }

    using Graph::disconnectNode;
    using Graph::disconnectEdge;
    using Graph::clearDisconnectedElements;

    ~Cfa();

private:
    Variable* createMemberVariable(const std::string& name, Type& type);
    Variable* findVariableByName(const std::vector<Variable*>& vec, llvm::StringRef name) const;

private:
    std::string mName;
    GazerContext& mContext;
    AutomataSystem& mParent;

    llvm::SmallVector<Location*, 1> mErrorLocations;
    llvm::SmallDenseMap<Location*, ExprPtr, 1> mErrorFieldExprs;

    std::vector<Variable*> mInputs;
    std::vector<Variable*> mOutputs;
    std::vector<Variable*> mLocals;

    Location* mEntry;
    Location* mExit;

    llvm::DenseMap<Variable*, std::string> mSymbolNames;
    llvm::DenseMap<unsigned, Location*> mLocationNumbers;

    Cfa* mParentAutomaton = nullptr;

    unsigned mLocationIdx = 0;
    unsigned mTmp = 0;
};

/// A system of CFA instances.
class AutomataSystem final
{
public:
    explicit AutomataSystem(GazerContext& context);

    AutomataSystem(const AutomataSystem&) = delete;
    AutomataSystem& operator=(const AutomataSystem&) = delete;

public:
    Cfa* createCfa(std::string name);

    using iterator = boost::indirect_iterator<std::vector<std::unique_ptr<Cfa>>::iterator>;
    using const_iterator = boost::indirect_iterator<std::vector<std::unique_ptr<Cfa>>::const_iterator>;

    iterator begin() { return mAutomata.begin(); }
    iterator end() { return mAutomata.end(); }

    const_iterator begin() const { return mAutomata.begin(); }
    const_iterator end() const { return mAutomata.end(); }

    llvm::iterator_range<iterator> automata() {
        return llvm::make_range(begin(), end());
    }

    GazerContext& getContext() { return mContext; }

    size_t getNumAutomata() const { return mAutomata.size(); }
    Cfa* getAutomatonByName(llvm::StringRef name) const;

    Cfa* getMainAutomaton() const {
        assert(mMainAutomata.size() == 1 && "The code asserts only one main automaton exists");
        return mMainAutomata[0];
    }

    llvm::iterator_range<std::vector<Cfa*>::const_iterator> mainAutomata() const {
        return llvm::make_range(mMainAutomata.cbegin(), mMainAutomata.cend());
    }
    void setMainAutomaton(Cfa* cfa);
    void addMainAutomaton(Cfa* cfa);

    void print(llvm::raw_ostream& os) const;

    using var_iterator = boost::indirect_iterator<std::vector<Variable*>::iterator>;

    var_iterator global_begin() { return mGlobals.begin(); }
    var_iterator global_end() { return mGlobals.end(); }
    llvm::iterator_range<var_iterator> globals() {
        return llvm::make_range(global_begin(), global_end());
    }

    /** Global variables contradict how MemorySSA works. */
    Variable* createGlobal(const std::string& name, Type &type) {
        // TODO name clashing?
        auto* var = mContext.createVariable(name, type);
        mGlobals.push_back(var);
        return var;
    }

    Variable* findGlobalVariable(llvm::StringRef name) const
    {
        Variable* variable = mContext.getVariable(name.str());
        if (variable == nullptr) {
            return nullptr;
        }

        if (std::find(mGlobals.begin(), mGlobals.end(), variable) == mGlobals.end()) {
            return nullptr;
        }

        return variable;
    }


private:
    GazerContext& mContext;
    std::vector<std::unique_ptr<Cfa>> mAutomata;
    std::vector<Cfa*> mMainAutomata;

    std::vector<Variable*> mGlobals;
};

inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Transition& transition)
{
    transition.print(os);
    return os;
}

} // end namespace gazer


// GraphTraits specialization for automata
//-------------------------------------------------------------------------
namespace llvm
{

template<>
struct GraphTraits<gazer::Cfa> :
    public GraphTraits<gazer::Graph<gazer::Location, gazer::Transition>>
{
    static NodeRef getEntryNode(const gazer::Cfa& cfa) {
        return cfa.getEntry();
    }
};

template<>
struct GraphTraits<Inverse<gazer::Cfa>> :
    public GraphTraits<Inverse<gazer::Graph<gazer::Location, gazer::Transition>>>
{
    static NodeRef getEntryNode(Inverse<gazer::Cfa>& cfa) {
        return cfa.Graph.getExit();
    }
};

template<>
struct GraphTraits<gazer::Location*> :
    public GraphTraits<gazer::GraphNode<gazer::Location, gazer::Transition>*>
{};

template<>
struct GraphTraits<Inverse<gazer::Location*>> :
    public GraphTraits<Inverse<gazer::GraphNode<gazer::Location, gazer::Transition>*>>
{};

} // end namespace llvm

#endif

