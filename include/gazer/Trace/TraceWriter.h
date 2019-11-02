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