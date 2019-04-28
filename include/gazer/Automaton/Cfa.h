#ifndef _GAZER_AUTOMATON_CFA_H
#define _GAZER_AUTOMATON_CFA_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/DenseMap.h>
#include <boost/iterator/indirect_iterator.hpp>

namespace gazer
{

class Cfa;
class Transition;

class Location
{
    friend class Cfa;
    using EdgeVectorTy = std::vector<Transition*>;
private:
    explicit Location(unsigned id)
        : mID(id)
    {}

public:
    Location(const Location&) = delete;
    Location& operator=(const Location&) = delete;

    unsigned getId() const { return mID; }

    size_t getNumIncoming() const { return mIncoming.size(); }
    size_t getNumOutgoing() const { return mOutgoing.size(); }

    //-------------------------- Iterator support ---------------------------//
    using edge_iterator = EdgeVectorTy::iterator;
    using const_edge_iterator = EdgeVectorTy::const_iterator;

    edge_iterator incoming_begin() { return mIncoming.begin(); }
    edge_iterator incoming_end() { return mIncoming.end(); }
    llvm::iterator_range<edge_iterator> incoming() {
        return llvm::make_range(incoming_begin(), incoming_end());
    }

    edge_iterator outgoing_begin() { return mOutgoing.begin(); }
    edge_iterator outgoing_end() { return mOutgoing.end(); }
    llvm::iterator_range<edge_iterator> outgoing() {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

    const_edge_iterator incoming_begin() const { return mIncoming.begin(); }
    const_edge_iterator incoming_end() const { return mIncoming.end(); }
    llvm::iterator_range<const_edge_iterator> incoming() const {
        return llvm::make_range(incoming_begin(), incoming_end());
    }

    const_edge_iterator outgoing_begin() const { return mOutgoing.begin(); }
    const_edge_iterator outgoing_end() const { return mOutgoing.end(); }
    llvm::iterator_range<const_edge_iterator> outgoing() const {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

private:
    void addIncoming(Transition* edge);
    void addOutgoing(Transition* edge);

    void removeIncoming(Transition* edge);
    void removeOutgoing(Transition* edge);

private:
    EdgeVectorTy mIncoming;
    EdgeVectorTy mOutgoing;
    unsigned mID;
};

/// A simple transition with a guard or summary expression.
class Transition
{
public:
    enum EdgeKind
    {
        Edge_Assign,    ///< Variable assignment.
        Edge_Call,      ///< Call into another procedure.
    };

protected:
    Transition(Location* source, Location* target, ExprPtr expr, EdgeKind kind)
        : mSource(source), mTarget(target), mExpr(expr), mEdgeKind(kind)
    {
        assert(source != nullptr && "Transition source location must not be null!");
        assert(target != nullptr && "Transition target location must not be null!");
        assert(expr != nullptr && "Transition guard expression must not be null!");
        assert(expr->getType().isBoolType()
            && "Transition guards can only be booleans!");
    }
public:
    Transition(Transition&) = delete;
    Transition& operator=(Transition&) = delete;

    Location* getSource() const { return mSource; }
    Location* getTarget() const { return mTarget; }

    ExprPtr getExpr() const { return mExpr; }
    EdgeKind getKind() const { return mEdgeKind; }

    bool isAssign() const { return mEdgeKind == Edge_Assign; }
    bool isCall() const { return mEdgeKind == Edge_Call; }

    void print(llvm::raw_ostream& os) const;

    virtual ~Transition() = default;

private:
    Location* mSource;
    Location* mTarget;
    ExprPtr mExpr;
    EdgeKind mEdgeKind;
};

class VariableAssignment final
{
public:
    VariableAssignment(Variable *variable, ExprPtr value)
        : mVariable(variable), mValue(value)
    {
        assert(variable->getType() == value->getType());
    }

    Variable* getVariable() const { return mVariable; }
    ExprPtr getValue() const { return mValue; }

    void print(llvm::raw_ostream& os) const;

private:
    Variable* mVariable;
    ExprPtr mValue;
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
    using arg_iterator = std::vector<VariableAssignment>::const_iterator;

    arg_iterator input_begin() const { return mInputArgs.begin(); }
    arg_iterator input_end() const { return mInputArgs.end(); }
    llvm::iterator_range<arg_iterator> inputs() const {
        return llvm::make_range(input_begin(), input_end());
    }
    size_t getNumInputs() const { return mInputArgs.size(); }

    arg_iterator output_begin() const { return mOutputArgs.begin(); }
    arg_iterator output_end() const { return mOutputArgs.end(); }
    llvm::iterator_range<arg_iterator> outputs() const {
        return llvm::make_range(output_begin(), output_end());
    }
    size_t getNumOutputs() const { return mOutputArgs.size(); }

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
class Cfa final
{
    friend class AutomataSystem;
private:
    Cfa(GazerContext& context, std::string name, AutomataSystem* parent);

public:
    Cfa(const Cfa&) = delete;
    Cfa& operator=(const Cfa&) = delete;

public:
    //------------------------- Locations and edges -------------------------//
    Location* createLocation();

    AssignTransition* createAssignTransition(Location* source, Location* target);

    AssignTransition* createAssignTransition(
        Location* source, Location* target,
        ExprPtr guard, std::vector<VariableAssignment> assignments
    );

    AssignTransition* createAssignTransition(
        Location* source, Location* target, std::vector<VariableAssignment> assignments
    );

    AssignTransition* createAssignTransition(
        Location* source, Location* target, ExprPtr guard
    );

    CallTransition* createCallTransition(
        Location* source, Location* target, ExprPtr guard,
        Cfa* callee, std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs
    );

    CallTransition* createCallTransition(
        Location* source, Location* target,
        Cfa* callee, std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs
    );

    Variable* addInput(llvm::StringRef name, Type& type);
    Variable* addOutput(llvm::StringRef name, Type& type);
    Variable* addLocal(llvm::StringRef name, Type& type);

    //-------------------------- Iterator support ---------------------------//
    using node_iterator = std::vector<std::unique_ptr<Location>>::iterator;
    using const_node_iterator = std::vector<std::unique_ptr<Location>>::const_iterator;

    node_iterator node_begin() { return mLocations.begin(); }
    node_iterator node_end() { return mLocations.end(); }

    const_node_iterator node_begin() const { return mLocations.begin(); }
    const_node_iterator node_end() const { return mLocations.end(); }
    llvm::iterator_range<node_iterator> nodes() {
        return llvm::make_range(node_begin(), node_end());
    }
    llvm::iterator_range<const_node_iterator> nodes() const {
        return llvm::make_range(node_begin(), node_end());
    }

    // Transition (edge) iterators...
    using edge_iterator = std::vector<std::unique_ptr<Transition>>::iterator;
    using const_edge_iterator = std::vector<std::unique_ptr<Transition>>::const_iterator;

    edge_iterator edge_begin() { return mTransitions.begin(); }
    edge_iterator edge_end() { return mTransitions.end(); }

    const_edge_iterator edge_begin() const { return mTransitions.begin(); }
    const_edge_iterator edge_end() const { return mTransitions.end(); }
    llvm::iterator_range<edge_iterator> edges() {
        return llvm::make_range(edge_begin(), edge_end());
    }
    llvm::iterator_range<const_edge_iterator> edges() const {
        return llvm::make_range(edge_begin(), edge_end());
    }

    // Nested automata support
    using nested_automata_iterator = std::vector<Cfa*>::iterator;
    llvm::iterator_range<nested_automata_iterator> nestedAutomata() {
        return llvm::make_range(mNestedAutomata.begin(), mNestedAutomata.end());
    }

    using var_iterator = boost::indirect_iterator<std::vector<Variable*>::iterator>;
    llvm::iterator_range<var_iterator> inputs() {
        return llvm::make_range(mInputs.begin(), mInputs.end());
    }

    llvm::iterator_range<var_iterator> outputs() {
        return llvm::make_range(mOutputs.begin(), mOutputs.end());
    }

    llvm::iterator_range<var_iterator> locals() {
        return llvm::make_range(mLocals.begin(), mLocals.end());
    }

    //------------------------------- Others --------------------------------//
    llvm::StringRef getName() const { return mName; }
    Location* getEntry() const { return mEntry; }
    Location* getExit() const { return mExit; }

    size_t getNumLocations() const { return mLocations.size(); }
    size_t getNumTransitions() const { return mTransitions.size(); }

    size_t getNumInputs() const { return mInputs.size(); }
    size_t getNumOutputs() const { return mOutputs.size(); }
    size_t getNumLocals() const { return mLocals.size(); }

    size_t getNumNestedAutomata() const { return mNestedAutomata.size(); }

    /// View the graph representation of this CFA with the
    /// system's default GraphViz viewer.
    void view() const;

private:
    Variable* createMemberVariable(llvm::Twine name, Type& type);

    /// Inserts the given automaton into this object as nested automaton.
    void addNestedAutomaton(Cfa* cfa);

private:
    std::string mName;

    std::vector<std::unique_ptr<Location>> mLocations;
    std::vector<std::unique_ptr<Transition>> mTransitions;

    std::vector<Variable*> mInputs;
    std::vector<Variable*> mOutputs;
    std::vector<Variable*> mLocals;

    Location* mEntry;
    Location* mExit;

    Cfa* mParentAutomaton = nullptr;
    std::vector<Cfa*> mNestedAutomata;

    GazerContext& mContext;
    unsigned int mLocationIdx = 0;
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
    Cfa* createNestedCfa(Cfa* parent, std::string name);

    void addGlobalVariable(Variable* variable);

    using iterator = boost::indirect_iterator<std::vector<std::unique_ptr<Cfa>>::iterator>;
    using const_iterator = boost::indirect_iterator<std::vector<std::unique_ptr<Cfa>>::const_iterator>;

    iterator begin() { return mAutomata.begin(); }
    iterator end() { return mAutomata.end(); }

    const_iterator begin() const { return mAutomata.begin(); }
    const_iterator end() const { return mAutomata.end(); }

    GazerContext& getContext() { return mContext; }

private:
    GazerContext& mContext;
    std::vector<std::unique_ptr<Cfa>> mAutomata;
    std::vector<Variable*> mGlobalVariables;
};


} // end namespace gazer


// GraphTraits specialization for automata
//-------------------------------------------------------------------------
namespace llvm
{

template<>
struct GraphTraits<gazer::Cfa>
{
    using NodeRef = gazer::Location*;
    using EdgeRef = gazer::Transition*;

    static constexpr auto GetEdgeTarget = [](const gazer::Transition* edge) -> NodeRef  {
        return edge->getTarget();
    };
    static constexpr auto GetLocationFromPtr = [](const std::unique_ptr<gazer::Location>& loc)
        -> gazer::Location*
    {
        return loc.get();
    };

    // Child traversal
    using ChildIteratorType = llvm::mapped_iterator<
        gazer::Location::edge_iterator,
        decltype(GetEdgeTarget),
        NodeRef
    >;

    static ChildIteratorType child_begin(NodeRef loc) {
        return ChildIteratorType(loc->outgoing_begin(), GetEdgeTarget);
    }
    static ChildIteratorType child_end(NodeRef loc) {
        return ChildIteratorType(loc->outgoing_end(), GetEdgeTarget);
    }

    using nodes_iterator = llvm::mapped_iterator<
        gazer::Cfa::const_node_iterator, decltype(GetLocationFromPtr)
    >;

    static nodes_iterator nodes_begin(const gazer::Cfa& cfa) {
        return nodes_iterator(cfa.node_begin(), GetLocationFromPtr);
    }
    static nodes_iterator nodes_end(const gazer::Cfa& cfa) {
        return nodes_iterator(cfa.node_end(), GetLocationFromPtr);
    }

    static NodeRef getEntryNode(gazer::Cfa& cfa) {
        return cfa.getEntry();
    }

    // Edge traversal
    using ChildEdgeIteratorType = gazer::Location::edge_iterator;
    static ChildEdgeIteratorType child_edge_begin(NodeRef loc) {
        return loc->outgoing_begin();
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef loc) {
        return loc->outgoing_end();
    }
    static NodeRef edge_dest(EdgeRef edge) {
        return edge->getTarget();
    }

    static unsigned size(gazer::Cfa& cfa) {
        return cfa.getNumLocations();
    }
};

}

#endif

