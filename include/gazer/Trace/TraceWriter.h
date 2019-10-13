#ifndef GAZER_TRACE_TRACEWRITER_H
#define GAZER_TRACE_TRACEWRITER_H

#include "gazer/Trace/Trace.h"

namespace gazer
{

/// Writes the contents of a trace into an output stream.
class TraceWriter : public TraceEventVisitor<void>
{
public:
    explicit TraceWriter(llvm::raw_ostream& os)
        : mOS(os)
    {}

    void write(Trace& trace) {
        for (auto& event : trace) {
            event->accept(*this);
        }
    }

protected:
    llvm::raw_ostream& mOS;
};

namespace trace
{
    std::unique_ptr<TraceWriter> CreateTextWriter(llvm::raw_ostream& os, bool printBv = true);
}

} // end namespace gazer

#endif