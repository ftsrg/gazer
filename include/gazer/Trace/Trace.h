#ifndef GAZER_TRACE_TRACE_H
#define GAZER_TRACE_TRACE_H

#include "gazer/Trace/Location.h"
#include "gazer/Core/Expr.h"

#include <llvm/Support/ErrorHandling.h>

#include <utility>
#include <vector>

namespace gazer
{
class Valuation;

template<class ReturnT = void>
class TraceEventVisitor;

/// Represents a step in a counterexample trace.
class TraceEvent
{
public:
    enum EventKind
    {
        Event_Assign,
        Event_FunctionEntry,
        Event_FunctionReturn,
        Event_FunctionCall,
        Event_UndefinedBehavior
    };

protected:
    explicit TraceEvent(EventKind kind, LocationInfo location = {})
        : mKind(kind), mLocation(location)
    {}
public:
    TraceEvent(const TraceEvent&) = delete;
    TraceEvent& operator=(const TraceEvent&) = delete;

public:
    EventKind getKind() const { return mKind; }
    LocationInfo getLocation() const { return mLocation; }

    template<class ReturnT>
    ReturnT accept(TraceEventVisitor<ReturnT>& visitor);

    virtual ~TraceEvent() = default;

private:
    EventKind mKind;
    LocationInfo mLocation;
};

/// Represents a detailed counterexample trace, possibly provided by
/// a verification algorithm.
class Trace
{
public:
    explicit Trace(std::vector<std::unique_ptr<TraceEvent>> events)
        : mEvents(std::move(events))
    {}
public:
    Trace(const Trace&) = delete;
    Trace& operator=(const Trace&) = delete;

public:
    // Iterator support for iterating events
    using iterator = std::vector<std::unique_ptr<TraceEvent>>::iterator;
    using const_iterator = std::vector<std::unique_ptr<TraceEvent>>::const_iterator;

    iterator begin() { return mEvents.begin(); }
    iterator end() { return mEvents.end(); }
    const_iterator begin() const { return mEvents.begin(); }
    const_iterator end() const { return mEvents.end(); }
    
private:
    std::vector<std::unique_ptr<TraceEvent>> mEvents;
};

template<class State, class Action>
class TraceBuilder
{
public:
    /// Builds a trace from a sequence of states and actions.
    virtual std::unique_ptr<Trace> build(
        std::vector<State>& states,
        std::vector<Action>& actions
    ) = 0;
    
    virtual ~TraceBuilder() = default;
};

/// Indicates an assignment in the original program.
class AssignTraceEvent final : public TraceEvent
{
public:
    AssignTraceEvent(
        std::string variableName,
        ExprRef<AtomicExpr> expr,
        LocationInfo location = {}
    ) : TraceEvent(TraceEvent::Event_Assign, location),
        mVariableName(std::move(variableName)), mExpr(std::move(expr))
    {}

    std::string getVariableName() const { return mVariableName; }
    ExprRef<AtomicExpr> getExpr() const { return mExpr; }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_Assign;
    }
private:
    std::string mVariableName;
    ExprRef<AtomicExpr> mExpr;
};

/// Indicates an entry into a procedure.
class FunctionEntryEvent final : public TraceEvent
{
public:
    explicit FunctionEntryEvent(std::string functionName, LocationInfo location = {})
        : TraceEvent(TraceEvent::Event_FunctionEntry, location),
        mFunctionName(std::move(functionName))
    {}

    std::string getFunctionName() const { return mFunctionName; }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_FunctionEntry;
    }
private:
    std::string mFunctionName;
};

class FunctionReturnEvent : public TraceEvent
{
public:
    FunctionReturnEvent(
        std::string functionName,
        ExprRef<AtomicExpr> returnValue,
        LocationInfo location = {}
    ) : TraceEvent(TraceEvent::Event_FunctionReturn, location),
        mFunctionName(std::move(functionName)), mReturnValue(std::move(returnValue))
    {}

    std::string getFunctionName() const { return mFunctionName; }
    ExprRef<AtomicExpr> getReturnValue() const { return mReturnValue; }
    bool hasReturnValue() const { return mReturnValue != nullptr; }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_FunctionReturn;
    }

private:
    std::string mFunctionName;
    ExprRef<AtomicExpr> mReturnValue;
};

/// Indicates a call to a nondetermistic function.
class FunctionCallEvent : public TraceEvent
{
    using ArgsVectorTy = std::vector<ExprRef<AtomicExpr>>;
public:
    FunctionCallEvent(
        std::string functionName,
        ExprRef<AtomicExpr> returnValue,
        ArgsVectorTy args = {},
        LocationInfo location = {}   
    ) : TraceEvent(TraceEvent::Event_FunctionCall, location),
        mFunctionName(functionName), mReturnValue(returnValue), mArgs(args)
    {}

    std::string getFunctionName() const { return mFunctionName; }
    ExprRef<AtomicExpr> getReturnValue() const { return mReturnValue; }

    using arg_iterator = ArgsVectorTy::iterator;
    arg_iterator arg_begin() { return mArgs.begin(); }
    arg_iterator arg_end() { return mArgs.end(); }
    llvm::iterator_range<arg_iterator> args() {
        return llvm::make_range(arg_begin(), arg_end());
    }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_FunctionCall;
    }

private:
    std::string mFunctionName;
    ExprRef<AtomicExpr> mReturnValue;
    ArgsVectorTy mArgs;
};

/// Indicates that undefined behavior has occured.
class UndefinedBehaviorEvent : public TraceEvent
{
public:
    explicit UndefinedBehaviorEvent(
        ExprRef<AtomicExpr> pickedValue,
        LocationInfo location = {}
    ) : TraceEvent(TraceEvent::Event_UndefinedBehavior, location),
        mPickedValue(std::move(pickedValue))
    {}

    ExprRef<AtomicExpr> getPickedValue() const { return mPickedValue; }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_UndefinedBehavior;
    }
private:
    ExprRef<AtomicExpr> mPickedValue;
};

template<class ReturnT>
class TraceEventVisitor
{
public:
    virtual ReturnT visit(AssignTraceEvent& event) = 0;
    virtual ReturnT visit(FunctionEntryEvent& event) = 0;
    virtual ReturnT visit(FunctionReturnEvent& event) = 0;
    virtual ReturnT visit(FunctionCallEvent& event) = 0;
    virtual ReturnT visit(UndefinedBehaviorEvent& event) = 0;

    virtual ~TraceEventVisitor() = default;
};

template<class ReturnT>
inline ReturnT TraceEvent::accept(TraceEventVisitor<ReturnT>& visitor)
{
    switch (mKind) {
        case Event_Assign:
            return visitor.visit(*llvm::cast<AssignTraceEvent>(this));
        case Event_FunctionEntry:
            return visitor.visit(*llvm::cast<FunctionEntryEvent>(this));
        case Event_FunctionReturn:
            return visitor.visit(*llvm::cast<FunctionReturnEvent>(this));
        case Event_FunctionCall:
            return visitor.visit(*llvm::cast<FunctionCallEvent>(this));
        case Event_UndefinedBehavior:
            return visitor.visit(*llvm::cast<UndefinedBehaviorEvent>(this));
    }

    llvm_unreachable("Unknown TraceEvent kind!");
}

} // end namespace gazer

#endif