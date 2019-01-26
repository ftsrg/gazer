#ifndef _GAZER_AUTOMATON_CFA_H
#define _GAZER_AUTOMATON_CFA_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"

namespace gazer
{

class Cfa;
class Transition;

class Location
{
    using EdgeVectorTy = std::vector<Transition*>;
private:
    Location(unsigned id)
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
    friend class Cfa;
public:
    enum EdgeKind
    {
        Edge_Simple,    ///< A simple transition with a possible guard expression.
        Edge_Assign,    ///< Variable assignment.
        Edge_Call,      ///< Call into another procedure.
    };

protected:
    Transition(Location* source, Location* target, ExprPtr expr, EdgeKind kind)
        : mSource(source), mTarget(target), mExpr(expr), mEdgeKind(kind)
    {}
public:
    Transition(Transition&) = delete;
    Transition& operator=(Transition&) = delete;

    Location* getSource() const { return mSource; }
    Location* getTarget() const { return mTarget; }

    ExprPtr getExpr() const { return mExpr; }

    EdgeKind getKind() const { return mEdgeKind; }

    bool isSummary() const { return mEdgeKind == Edge_Simple; }
    bool isCall() const { return mEdgeKind == Edge_Call; }
    bool isAssign() const { return mEdgeKind == Edge_Assign; }

    void print(llvm::raw_ostream& os) const;

    virtual ~Transition() {}

private:
    Location* mSource;
    Location* mTarget;
    ExprPtr mExpr;
    EdgeKind mEdgeKind;
};

class VariableAssignment final
{
public:
    VariableAssignment(Variable *mVariable, ExprPtr mValue)
        : mVariable(mVariable), mValue(mValue)
    {}

    Variable* getVariable() const { return mVariable; }
    ExprPtr getValue() const { return mValue; }

private:
    Variable* mVariable;
    ExprPtr mValue;
};

/// Represents a (potentially guared) transition with variable assignments.
class AssignTransition final : public Transition
{
protected:
    AssignTransition(Location* source, Location* target, std::vector<VariableAssignment> assignments);

public:
    using iterator = std::vector<VariableAssignment>::const_iterator;
    iterator begin() { return mAssignments.begin(); }
    iterator end() { return mAssignments.end(); }

private:
    std::vector<VariableAssignment> mAssignments;
};

/// Represents a (potentially guarded) transition with a procedure call.
class CallTransition final : public Transition
{
protected:
    CallTransition(
        Location* source, Location* target,
        Cfa* callee,
        std::vector<ExprPtr> inputArgs,
        std::vector<Variable*> outputArgs
    );

public:
    Cfa* getCalledAutomaton() const { return mCallee; }

    //-------------------------- Iterator support ---------------------------//
    using arg_iterator = std::vector<ExprPtr>::const_iterator;
    arg_iterator arg_begin() const { return mInputArgs.begin(); }
    arg_iterator arg_end() const { return mInputArgs.end(); }
    llvm::iterator_range<arg_iterator> args() const {
        return llvm::make_range(arg_begin(), arg_end());
    }
    size_t getNumArgs() const { return mInputArgs.size(); }

    using output_iterator = std::vector<Variable*>::const_iterator;
    output_iterator output_begin() const { return mOutputArgs.begin(); }
    output_iterator output_end() const { return mOutputArgs.end(); }
    llvm::iterator_range<output_iterator> outputs() const {
        return llvm::make_range(output_begin(), output_end());
    }
    size_t getNumOutputs() const { return mOutputArgs.size(); }

private:
    Cfa* mCallee;
    std::vector<ExprPtr> mInputArgs;
    std::vector<Variable*> mOutputArgs;
};

/// Represents a control flow automaton.
class Cfa : public SymbolScope
{
    using LocationVectorTy = std::vector<std::unique_ptr<Location>>;
    using TransitionVectorTy = std::vector<std::unique_ptr<Transition>>;
    using VariablesVectorTy = std::vector<Variable*>;
public:

private:
    Location* mEntry;
    Location* mExit;
};

}

#endif

