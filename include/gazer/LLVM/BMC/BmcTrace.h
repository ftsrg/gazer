#ifndef _GAZER_LLVM_ANALYSIS_BMCTRACE_H
#define _GAZER_LLVM_ANALYSIS_BMCTRACE_H

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Valuation.h"
#include "gazer/LLVM/Ir2Expr.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>

#include <vector>
#include <string>

namespace llvm {
    class raw_ostream;
}

namespace gazer
{

class BmcTraceWriter;

/**
 * Represents a sequential trace from a BMC algorithm.
 */ 
class BmcTrace
{
public:
    class LocationInfo
    {
    public:
        LocationInfo(unsigned int line = 0, unsigned int column = 0)
            : mLine(line), mColumn(column)
        {}

        LocationInfo(const LocationInfo&) = default;

        int getLine() const { return mLine; }
        int getColumn() const { return mColumn; }
    private:
        int mLine;
        int mColumn;
    };

    class Event
    {
    public:
        enum EventKind
        {
            Assign,
            FunctionEntry,
            ArgumentValue,
            FunctionCall
        };

    protected:
        Event(EventKind kind, LocationInfo location = {0, 0})
            : mKind(kind), mLocation(location)
        {}
   
    public:
        virtual void write(BmcTraceWriter& writer) = 0;
        virtual ~Event() {}

        EventKind getKind() const { return mKind; }
        LocationInfo getLocation() const { return mLocation; }

    private:
        EventKind mKind;
        LocationInfo mLocation;
    };

    class AssignmentEvent : public Event
    {
    public:
        AssignmentEvent(
            std::string variable,
            std::shared_ptr<LiteralExpr> expr,
            LocationInfo location = {0, 0}
        ) : Event(Assign, location), mVariableName(variable), mExpr(expr)
        {}

        void write(BmcTraceWriter& writer) override;

        std::string getVariableName() const { return mVariableName; }
        std::shared_ptr<LiteralExpr> getExpr() const { return mExpr; }

        static bool classof(const Event* event) {
            return event->getKind() == Event::Assign;
        }

    private:
        std::string mVariableName;
        std::shared_ptr<LiteralExpr> mExpr;
    };

    class FunctionEntryEvent : public Event
    {
    public:
        FunctionEntryEvent(
            std::string functionName,
            LocationInfo location = {0, 0}
        ) : Event(FunctionEntry, location), mFunctionName(functionName)
        {}

        void write(BmcTraceWriter& writer) override;

        std::string getFunctionName() const { return mFunctionName; }
        LocationInfo getLocation() const { return mLocation; }

        static bool classof(const Event* event) {
            return event->getKind() == Event::FunctionEntry;
        }

    private:
        std::string mFunctionName;
        LocationInfo mLocation;
    };

    class ArgumentValueEvent : public Event
    {
    public:
        ArgumentValueEvent(std::string argname, std::shared_ptr<LiteralExpr> value)
            : Event(ArgumentValue), mArgumentName(argname), mValue(value)
        {}

        void write(BmcTraceWriter& writer) override;

        std::shared_ptr<LiteralExpr> getValue() const { return mValue; }

    private:
        std::string mArgumentName;
        std::shared_ptr<LiteralExpr> mValue;
    };

    class FunctionCallEvent : public Event
    {
        using ArgsVectorT = std::vector<std::shared_ptr<LiteralExpr>>;
    public:
        FunctionCallEvent(
            llvm::Function* function,
            std::shared_ptr<LiteralExpr> returnValue,
            std::vector<std::shared_ptr<LiteralExpr>> args = {},
            LocationInfo location = {0, 0}
        ) : Event(FunctionCall, location), mFunction(function), 
        mReturnValue(returnValue), mArgs(args)
        {}

        void write(BmcTraceWriter& writer) override;

        llvm::Function* getFunction() const { return mFunction; }
        std::shared_ptr<LiteralExpr> getReturnValue() const { return mReturnValue; }
        
        using arg_iterator = ArgsVectorT::iterator;
        arg_iterator arg_begin() { return mArgs.begin(); }
        arg_iterator arg_end() { return mArgs.end(); }
        llvm::iterator_range<arg_iterator> args() {
            return llvm::make_range(arg_begin(), arg_end());
        }

        static bool classof(const Event* event) {
            return event->getKind() == Event::FunctionCall;
        }

    private:
        llvm::Function* mFunction;
        std::shared_ptr<LiteralExpr> mReturnValue;
        ArgsVectorT mArgs;
    };
public:
    BmcTrace(
        std::vector<std::unique_ptr<BmcTrace::Event>>& events,
        std::vector<llvm::BasicBlock*> blockTrace = {}
    ) : mEvents(std::move(events)), mBlocks(blockTrace)
    {}

    static std::unique_ptr<BmcTrace> Create(
        TopologicalSort& topo,
        llvm::DenseMap<llvm::BasicBlock*, size_t>& blocks,
        llvm::DenseMap<llvm::BasicBlock*, llvm::Value*>& preds,
        llvm::BasicBlock* errorBlock,
        Valuation& model,
        const InstToExpr::ValueToVariableMapT& valueMap
    );

public:
    using iterator = std::vector<std::unique_ptr<BmcTrace::Event>>::iterator;
    using block_iterator = std::vector<llvm::BasicBlock*>::iterator;

    iterator begin() { return mEvents.begin(); }
    iterator end() { return mEvents.end(); }
    llvm::iterator_range<iterator> events() {
        return llvm::make_range(begin(), end());
    }

    block_iterator block_begin() { return mBlocks.begin(); }
    block_iterator block_end() { return mBlocks.end(); }
    llvm::iterator_range<block_iterator> blocks() {
        return llvm::make_range(block_begin(), block_end());
    }

private:
    unsigned mErrorCode;
    std::vector<std::unique_ptr<BmcTrace::Event>> mEvents;
    std::vector<llvm::BasicBlock*> mBlocks;
};

class BmcTraceWriter
{
public:
    BmcTraceWriter(llvm::raw_ostream& os)
        : mOS(os)
    {}

public:
    virtual void write(BmcTrace& trace)
    {
        for (auto& event : trace) {
            event->write(*this);
        }
    }

    virtual void writeEvent(BmcTrace::AssignmentEvent& event) = 0;
    virtual void writeEvent(BmcTrace::FunctionEntryEvent& event) = 0;
    virtual void writeEvent(BmcTrace::ArgumentValueEvent& event) = 0;
    virtual void writeEvent(BmcTrace::FunctionCallEvent& event) = 0;

    virtual ~BmcTraceWriter() {}
protected:
    llvm::raw_ostream& mOS;
};

namespace bmc {
    std::unique_ptr<BmcTraceWriter> CreateTextTraceWriter(llvm::raw_ostream& os);
}

}

#endif
