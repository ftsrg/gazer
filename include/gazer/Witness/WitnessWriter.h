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
#ifndef GAZER_WITNESS_WITNESS_H
#define GAZER_WITNESS_WITNESS_H

#include <string>
#include <memory>

#include "gazer/Trace/TraceWriter.h"
#include "gazer/Trace/Trace.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfoMetadata.h>

namespace gazer {

// Although it sounds like that this class should be more closely related to the ViolationWitnessWriter, they are not,
// as this one only outputs a hardcoded empty correctness witness (for now) and has nothing to do with traces
class CorrectnessWitnessWriter {
    llvm::raw_ostream& os;
    std::string hash;
    
    static const std::string nodes;
    static const std::string schema;
    static const std::string keys;
    static const std::string graph_data;
public:
    static std::string src_filename;

    explicit CorrectnessWitnessWriter(llvm::raw_ostream& _os, std::string _hash)
    : os(_os), hash(_hash) {}

    void outputWitness();
};

class ViolationWitnessWriter : public TraceWriter {
public:
    static std::string src_filename;

    explicit ViolationWitnessWriter(llvm::raw_ostream& os, Trace& _trace, std::string _hash)
    : TraceWriter(os), trace(_trace), hash(_hash) {}

    // After creating the WitnessWriter, the witness should be initialized, written and then closed
    void initializeWitness();
    void closeWitness();

private:
    void visit(AssignTraceEvent& event) override;
    void visit(FunctionEntryEvent& event) override;
    void visit(FunctionReturnEvent& event) override;
    void visit(FunctionCallEvent& event) override;
    void visit(UndefinedBehaviorEvent& event) override;

    unsigned int nodeCounter = 0; // the values is always the id of the next node, that hasn't been created yet
    bool inProgress = false; // true, if witness is initialized, but not closed
    Trace& trace;
    const std::string hash; // The hash of the source file

    static const std::string schema;
    static const std::string keys;
    static const std::string graph_data;

    void createNode(bool violation = false);
    void openEdge();
    void closeEdge();
    void writeLocation(gazer::LocationInfo location); // should be used in edges
};

}

#endif