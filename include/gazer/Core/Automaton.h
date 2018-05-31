#ifndef _GAZER_CORE_CFA_H
#define _GAZER_CORE_CFA_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"
#include "gazer/Core/SymbolTable.h"

#include <llvm/ADT/iterator_range.h>

#include <fmt/format.h>

#include <vector>
#include <string>
#include <memory>
#include <iosfwd>

namespace gazer
{

class CfaEdge;
class Automaton;

/**
 * Represents a single location in an automaton.
 */
class Location final
{
    friend class Automaton;
private:
    Location(std::string name, const Automaton* parent)
        : mName(name), mParent(parent)
    {}

public:
    // Locations are non-copyable.
    Location(const Location&) = delete;
    Location& operator=(const Location&) = delete;

    std::string getName() const { return mName; }
    const Automaton* getParent() const { return mParent; }

    /**
     * Equality comparison.
     *
     * Two locations are considered equal, if they belong to the same automaton
     * and their names match.
     */
    bool operator==(const Location& rhs) const;
    bool operator!=(const Location& rhs) const { return !(*this == rhs); }

public:
    //------- Iterator access -------//
    using edge_iterator = typename std::vector<CfaEdge*>::iterator;
    using const_edge_iterator = typename std::vector<CfaEdge*>::const_iterator;

    edge_iterator incoming_begin() { return mIncoming.begin(); }
    edge_iterator incoming_end() { return mIncoming.end(); }
    const_edge_iterator incoming_begin() const { return mIncoming.begin(); }
    const_edge_iterator incoming_end() const { return mIncoming.end(); }

    edge_iterator outgoing_begin() { return mOutgoing.begin(); }
    edge_iterator outgoing_end() { return mOutgoing.end(); }
    const_edge_iterator outgoing_begin() const { return mOutgoing.begin(); }
    const_edge_iterator outgoing_end() const { return mOutgoing.end(); }

    llvm::iterator_range<edge_iterator> outgoing() {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

    llvm::iterator_range<edge_iterator> incoming() {
        return llvm::make_range(incoming_begin(), incoming_end());
    }
private:
    void addIncoming(CfaEdge* edge) {
        mIncoming.push_back(edge);
    }

    void addOutgoing(CfaEdge* edge) {
        mOutgoing.push_back(edge);
    }

private:
    std::string mName;
    const Automaton* mParent;
    std::vector<CfaEdge*> mIncoming;
    std::vector<CfaEdge*> mOutgoing;
};

/**
 * Represents an edge, going between two locations.
 */
class CfaEdge
{
    friend class Automaton;
public:
    enum EdgeKind
    {
        //Edge_Skip,
        //Edge_Assume,
        Edge_Assign,
        //Edge_Havoc,
        //Edge_Call,
        //Edge_Return
    };

protected:
    CfaEdge(EdgeKind type, Location& source, Location& target, ExprPtr guard = nullptr)
        : mKind(type), mSource(source), mTarget(target), mGuard(guard)
    {
        if (mGuard != nullptr) {
            assert(mGuard->getType().isBoolType()
                && "Only booleans can be edge guards.");
        }
    }

public:
    CfaEdge(const CfaEdge&) = delete;
    CfaEdge& operator=(const CfaEdge&) = delete;

    Location& getSource() const { return mSource; }
    Location& getTarget() const { return mTarget; }

    /**
     * Equality comparison.
     *
     * Two edges are considered equal, if both their sources and targets
     * are equal.
     */
    bool operator==(const CfaEdge& rhs) const;
    bool operator!=(const CfaEdge& rhs) const { return !(*this == rhs); }

    EdgeKind getKind() const { return mKind; }
    ExprPtr getGuard() const { return mGuard; }

    //bool isSkip()   const { return mKind == Edge_Skip; }
    //bool isAssume() const { return mKind == Edge_Assume; }
    bool isAssign() const { return mKind == Edge_Assign; }
    //bool isHavoc()  const { return mKind == Edge_Havoc; }
    //bool isCall() const     { return mKind == Edge_Call; }
    //bool isReturn() const   { return mKind == Edge_Return; }

    virtual void print(std::ostream& os) const = 0;
    virtual ~CfaEdge() {}

private:
    EdgeKind mKind;
    Location& mSource;
    Location& mTarget;
    ExprPtr mGuard;
};

/**
 * Represents an automaton edge containing assignments.
 */
class AssignEdge final : public CfaEdge
{
public:
    struct Assignment
    {
        Variable& variable;
        ExprPtr expr;

        Assignment(std::pair<Variable, ExprPtr> pair)
            : Assignment(pair.first, pair.second)
        {}

        Assignment(Variable& variable, ExprPtr expr)
            : variable(variable), expr(expr)
        {
            if (variable.getType() != expr->getType()) {
                throw TypeCastError(fmt::format(
                    "Cannot assign an expression type of {0}"
                    "to the variable '{1}' (type of {2}).",
                    variable.getName(), variable.getType().getName(), expr->getType().getName()
                ));
            }
        }

        Assignment(const Assignment&) = default;
        Assignment& operator=(const Assignment&) = default;
    };

protected:
    AssignEdge(Location& source, Location& target, std::vector<Assignment> assignments, ExprPtr guard = nullptr)
        : CfaEdge(Edge_Assign, source, target, guard), mAssignments(assignments)
    {}

public:
    static std::unique_ptr<AssignEdge> Create(
        Location& source, Location& target,
        ExprPtr guard)
    {
        return std::unique_ptr<AssignEdge>(new AssignEdge(source, target, {}, guard));
    }

    static std::unique_ptr<AssignEdge> Create(
        Location& source, Location& target,
        std::vector<Assignment> assignments = {},
        ExprPtr guard = nullptr)
    {
        return std::unique_ptr<AssignEdge>(new AssignEdge(source, target, assignments, guard));
    }

public:
    //---- Inherited functions ----//
    virtual void print(std::ostream& os) const override;

    //---- Assignments ----//
    size_t getNumAssignments() const { return mAssignments.size(); }
    void addAssignment(Variable& variable, ExprPtr value) {
        mAssignments.push_back({variable, value});
    }

    //----- Iterator access -----//
    using assign_iterator = std::vector<Assignment>::iterator;
    assign_iterator assign_begin() { return mAssignments.begin(); }
    assign_iterator assign_end() { return mAssignments.end(); }

    llvm::iterator_range<assign_iterator> assignments() {
        return llvm::make_range(assign_begin(), assign_end());
    }

    //---- Type inqueries ----//
    static bool classof(const CfaEdge* edge) {
        return edge->getKind() == Edge_Assign;
    }

    static bool classof(const CfaEdge& edge) {
        return edge.getKind() == Edge_Assign;
    }

private:
    std::vector<Assignment> mAssignments;
};

/**
 * Create a type-checked assignment.
 */
inline AssignEdge::Assignment mk_assign(Variable& variable, ExprPtr expr)
{
    return AssignEdge::Assignment(variable, expr);
}

/**
 * Output operators.
 */
std::ostream& operator<<(std::ostream& os, const Location& location);
std::ostream& operator<<(std::ostream& os, const CfaEdge& edge);

/**
 * A Control Flow Automaton (CFA) class.
 * Each automaton contains a set of locations and edges.
 * Furthermore, each automaton has its own symbol table.
 */
class Automaton final
{
public:
    Automaton(std::string entryName = "entry", std::string exitName = "exit") {
        mEntry = mLocs.emplace_back(new Location(entryName, this)).get();
        mExit  = mLocs.emplace_back(new Location(exitName, this)).get();
    }

    Automaton(const Automaton&) = delete;
    Automaton& operator=(const Automaton&) = delete;
public:
    SymbolTable& getSymbols() { return mSymbolTable; }

    Location& entry() { return *mEntry; }
    Location& exit()  { return *mExit; }

    Location& createLocation() {
        return createLocation(std::to_string(mTempCounter++));
    }
    Location& createLocation(std::string name);

    CfaEdge& insertEdge(std::unique_ptr<CfaEdge> edge);

    CfaEdge& skip(Location& source, Location& target);
    //CfaEdge& assign(Location& source, Location& target)

public:
    using loc_iterator = std::vector<std::unique_ptr<Location>>::iterator;
    using edge_iterator = typename std::vector<std::unique_ptr<CfaEdge>>::iterator;

    loc_iterator loc_begin() { return mLocs.begin(); }
    loc_iterator loc_end() { return mLocs.end(); }
    llvm::iterator_range<loc_iterator> locs() {
        return llvm::make_range(loc_begin(), loc_end());
    }

    edge_iterator edge_begin()  { return mEdges.begin(); }
    edge_iterator edge_end()    { return mEdges.end(); }
    llvm::iterator_range<edge_iterator> edges() {
        return llvm::make_range(edge_begin(), edge_end());
    }

private:
    SymbolTable mSymbolTable;
    std::vector<std::unique_ptr<Location>> mLocs;
    std::vector<std::unique_ptr<CfaEdge>> mEdges;
    Location* mEntry;
    Location* mExit;
    size_t mTempCounter = 1;
};

} // end namespace gazer

#endif
