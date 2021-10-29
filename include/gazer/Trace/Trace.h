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

/// A simple wrapper for a trace variable
class TraceVariable
{
public:
    enum Representation
    {
        Rep_Unknown,
        Rep_Bool,
        Rep_Char,
        Rep_Signed,
        Rep_Unsigned,
        Rep_Float
    };

    TraceVariable(std::string name, Representation representation, size_t size = 0)
        : mName(std::move(name)), mRepresentation(representation), mSize(size)
    {}

    [[nodiscard]] llvm::StringRef getName() const { return mName; }
    [[nodiscard]] Representation getRepresentation() const { return mRepresentation; }
    [[nodiscard]] size_t getSize() const { return mSize; }

private:
    std::string mName;
    Representation mRepresentation;
    size_t mSize;
};

/// Indicates an assignment in the original program.
class AssignTraceEvent final : public TraceEvent
{
public:
    AssignTraceEvent(
        TraceVariable variable,
        ExprRef<AtomicExpr> expr,
        LocationInfo location = {}
    ) : TraceEvent(TraceEvent::Event_Assign, location),
        mVariable(std::move(variable)), mExpr(std::move(expr))
    {
        assert(mExpr != nullptr);
    }

    [[nodiscard]] const TraceVariable& getVariable() const { return mVariable; }
    [[nodiscard]] ExprRef<AtomicExpr> getExpr() const { return mExpr; }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_Assign;
    }
private:
    TraceVariable mVariable;
    ExprRef<AtomicExpr> mExpr;
};

/// Indicates an entry into a procedure.
class FunctionEntryEvent final : public TraceEvent
{
public:
    explicit FunctionEntryEvent(
        std::string functionName,
        std::vector<ExprRef<AtomicExpr>> args = {},
        LocationInfo location = {}
    ) : TraceEvent(TraceEvent::Event_FunctionEntry, location),
        mFunctionName(std::move(functionName)),
        mArgs(std::move(args))
    {}

    [[nodiscard]] std::string getFunctionName() const { return mFunctionName; }

    using arg_iterator = std::vector<ExprRef<AtomicExpr>>::const_iterator;
    arg_iterator arg_begin() const { return mArgs.begin(); }
    arg_iterator arg_end() const { return mArgs.end(); }
    [[nodiscard]] llvm::iterator_range<arg_iterator> args() {
        return llvm::make_range(arg_begin(), arg_end());
    }

    static bool classof(const TraceEvent* event) {
        return event->getKind() == TraceEvent::Event_FunctionEntry;
    }
private:
    std::string mFunctionName;
    std::vector<ExprRef<AtomicExpr>> mArgs;
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
        mFunctionName(std::move(functionName)), mReturnValue(returnValue), mArgs(args)
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