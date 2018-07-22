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
        virtual void write(BmcTraceWriter& writer) = 0;
        virtual ~Event() {}
    };

    class AssignmentEvent : public Event
    {
    public:
        AssignmentEvent(
            std::string variable,
            std::shared_ptr<LiteralExpr> expr,
            LocationInfo location
        ) : mVariableName(variable), mExpr(expr), mLocation(location)
        {}

        void write(BmcTraceWriter& writer) override;

        std::string getVariableName() const { return mVariableName; }
        std::shared_ptr<LiteralExpr> getExpr() const { return mExpr; }
        LocationInfo getLocation() const { return mLocation; }
    private:
        std::string mVariableName;
        std::shared_ptr<LiteralExpr> mExpr;
        LocationInfo mLocation;
    };

    class FunctionEntryEvent : public Event
    {
    public:
        FunctionEntryEvent(
            std::string functionName,
            LocationInfo location = {0, 0}
        ) : mFunctionName(functionName), mLocation(location)
        {}

        void write(BmcTraceWriter& writer) override;

        std::string getFunctionName() const { return mFunctionName; }
        LocationInfo getLocation() const { return mLocation; }
    private:
        std::string mFunctionName;
        LocationInfo mLocation;
    };
public:
    BmcTrace(
        std::vector<std::unique_ptr<BmcTrace::Event>>& events,
        std::vector<llvm::BasicBlock*> blockTrace = {}
    ) : mEvents(std::move(events)), mBlocks(blockTrace)
    {}

    static BmcTrace Create(
        TopologicalSort& topo,
        llvm::DenseMap<llvm::BasicBlock*, size_t>& blocks,
        llvm::DenseMap<llvm::BasicBlock*, llvm::Value*>& preds,
        llvm::BasicBlock* errorBlock,
        Valuation& model,
        const InstToExpr::ValueToVariableMapT& valueMap
    );

public:
    using event_iterator = std::vector<std::unique_ptr<BmcTrace::Event>>::iterator;
    using block_iterator = std::vector<llvm::BasicBlock*>::iterator;

    event_iterator event_begin() { return mEvents.begin(); }
    event_iterator event_end() { return mEvents.end(); }
    llvm::iterator_range<event_iterator> events() {
        return llvm::make_range(event_begin(), event_end());
    }

    block_iterator block_begin() { return mBlocks.begin(); }
    block_iterator block_end() { return mBlocks.end(); }
    llvm::iterator_range<block_iterator> blocks() {
        return llvm::make_range(block_begin(), block_end());
    }

private:
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
        for (auto& event : trace.events()) {
            event->write(*this);
        }
    }

    virtual void writeEvent(BmcTrace::AssignmentEvent& event) = 0;
    virtual void writeEvent(BmcTrace::FunctionEntryEvent& event) = 0;

    virtual ~BmcTraceWriter() {}
protected:
    llvm::raw_ostream& mOS;
};

namespace bmc {
    std::unique_ptr<BmcTraceWriter> CreateTextTraceWriter(llvm::raw_ostream& os);
}

}

#endif
