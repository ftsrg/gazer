#ifndef _GAZER_TRACE_TRACE_H
#define _GAZER_TRACE_TRACE_H

#include "gazer/Trace/Location.h"
#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"

#include <llvm/Support/ErrorHandling.h>

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
        Event_FunctionCall
    };

protected:
    TraceEvent(EventKind kind, LocationInfo location = {})
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

    virtual ~TraceEvent() {}

private:
    EventKind mKind;
    LocationInfo mLocation;
};

/// Represents a detailed counterexample trace, possibly provided by
/// a verification algorithm.
class Trace final
{
    template<class State>
    friend class TraceBuilder;
private:
    Trace(std::vector<std::unique_ptr<TraceEvent>> events)
        : mEvents(std::move(events))
    {}
public:
    Trace(const Trace&);
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

template<class State>
class TraceBuilder
{
public:
    /// Builds a trace from a model.
    std::unique_ptr<Trace> build(Valuation& model) {
       return std::unique_ptr<Trace>(new Trace(this->buildEvents(model)));
    }

protected:
    /// Generates a list of events from a model.
    virtual std::vector<std::unique_ptr<TraceEvent>> buildEvents(
        Valuation& model, const std::vector<State>& states
    ) = 0;
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
        mVariableName(variableName), mExpr(expr)
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
    FunctionEntryEvent(std::string functionName, LocationInfo location = {})
        : TraceEvent(TraceEvent::Event_FunctionEntry, location),
        mFunctionName(functionName)
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
    mFunctionName(functionName), mReturnValue(returnValue)
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

template<class ReturnT>
class TraceEventVisitor
{
public:
    virtual ReturnT visit(AssignTraceEvent& event) = 0;
    virtual ReturnT visit(FunctionEntryEvent& event) = 0;
    virtual ReturnT visit(FunctionReturnEvent& event) = 0;
    virtual ReturnT visit(FunctionCallEvent& event) = 0;

    virtual ~TraceEventVisitor() {}
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
    }

    llvm_unreachable("Unknown TraceEvent kind!");
}

/*
template<>
inline void TraceEvent::accept<void>(TraceEventVisitor<void>& visitor)
{
    switch (mKind) {
        case Event_Assign:
            visitor.visit(*llvm::cast<AssignTraceEvent>(this));
        case Event_FunctionEntry:
            visitor.visit(*llvm::cast<FunctionEntryEvent>(this));
        case Event_FunctionCall:
            visitor.visit(*llvm::cast<FunctionCallEvent>(this));
    }
} */

} // end namespace gazer

#endif