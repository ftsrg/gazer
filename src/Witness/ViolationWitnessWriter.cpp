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

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Witness/WitnessWriter.h"
#include <llvm/ADT/SmallString.h>

#include <fstream>
#include <time.h>

namespace gazer {

std::string ViolationWitnessWriter::src_filename{};

void ViolationWitnessWriter::createNode(bool violation) {
    mOS << "\n<node id=\"N" << nodeCounter << "\">\n";
    if(nodeCounter == 0) mOS << "\t<data key=\"entry\">true</data>\n";
    if(violation) mOS << "\t<data key=\"violation\">true</data>\n";
    mOS << "</node>\n";
    nodeCounter++;
}

void ViolationWitnessWriter::openEdge() {
    assert(nodeCounter>0 && "Can't create witness edge, there is only one node");
    mOS << "\n<edge source=\"N" << nodeCounter-2 << "\" target=\"N" << nodeCounter-1 << "\">\n";
}

void ViolationWitnessWriter::closeEdge() {
    mOS << "</edge>\n";
}

void ViolationWitnessWriter::writeLocation(gazer::LocationInfo location) {
    if (location.getLine() != 0) {
        mOS << "\t<data key=\"startline\">" 
            << location.getLine()
            << "</data>\n"
            << "\t<data key=\"endline\">" 
            << location.getLine()
            << "</data>\n";
    }
}

void ViolationWitnessWriter::initializeWitness() {
    nodeCounter = 0;
    time_t rawtime;
    struct tm * ptm;
    time ( &rawtime );
    ptm = gmtime( &rawtime ); // UTC timestamp
    std::stringstream timestamp;
    timestamp << ptm->tm_year+1900 << "-";
    timestamp << std::setfill('0') << std::setw(2) << ptm->tm_mon+1 << "-"
              << std::setfill('0') << std::setw(2) << ptm->tm_mday << "T" 
              << std::setfill('0') << std::setw(2) << ptm->tm_hour%24 << ":" 
              << std::setfill('0') << std::setw(2) << ptm->tm_min << ":" 
              << std::setfill('0') << std::setw(2) << ptm->tm_sec;

    mOS << schema;
    mOS << keys;
    mOS << graph_data;
    mOS << "<data key=\"programhash\">" << hash << "</data>";
    mOS << "<data key=\"creationtime\">" << timestamp.str() << "</data>\n";
    mOS << "<data key=\"programfile\">" << src_filename << "</data>\n";
    createNode(); // entry node
    inProgress = true;
}

void ViolationWitnessWriter::closeWitness() {
    createNode(true);
    openEdge();
    // ide lenne jó esetleg beírni a violation linet TODO
    closeEdge();
    mOS << "</graph>\n</graphml>";
    inProgress = false;
}

void ViolationWitnessWriter::visit(AssignTraceEvent& event) {
    // assert(inProgress && "Witness should be initialized before write and closed after write");

    // ExprRef<AtomicExpr> expr = event.getExpr();
    // if(!llvm::isa<UndefExpr>(expr.get())) {
    //     createNode();
    //     openEdge();
    //     mOS << "\t<data key=\"assumption\">" << event.getVariable().getName() << "== ";

    //     // output variable value
    //     if(auto bv = llvm::dyn_cast<BvLiteralExpr>(expr)) {
    //         TraceVariable var = event.getVariable();
    //         unsigned varSize = var.getSize();

    //         switch (var.getRepresentation()) {
    //             case TraceVariable::Rep_Unknown:
    //                 {
    //                     llvm::SmallString<100> buffer;
    //                     bv->getValue().toStringUnsigned(buffer, /*radix=*/16);
    //                     mOS << "0x" << buffer;
    //                     // bv->print(mOS); uses form of #<value>bv, not C standard
    //                 }
    //                 break;
    //             case TraceVariable::Rep_Bool:
    //                 if (bv->isZero()) {
    //                     mOS << "false";
    //                 } else {
    //                     mOS << "true";
    //                 }
    //                 break;
    //             case TraceVariable::Rep_Signed:
    //                 bv->getValue().zextOrSelf(var.getSize()).print(mOS, /*isSigned=*/true);
    //                 break;
    //             case TraceVariable::Rep_Unsigned:
    //                 bv->getValue().zextOrSelf(var.getSize()).print(mOS, /*isSigned=*/false);
    //                 break;
    //         }
    //     }
    //     else {
    //         expr->print(mOS);
    //     }

    //     mOS << ";</data>\n";
    //     // writeLocation(event.getLocation());
    //     closeEdge();
    // }
}

// Megj. amúgy ilyen eventre még nem láttam példát a kimenetben
void ViolationWitnessWriter::visit(FunctionEntryEvent& event) {
    assert(inProgress && "Witness should be initialized before write and closed after write");
    createNode();
    openEdge();
    writeLocation(event.getLocation());
    mOS << "\t<data key=\"enterFunction\">" << event.getFunctionName() << "</data>\n";
    closeEdge();
}

// Megj. amúgy ilyen eventre még nem láttam példát a kimenetben
void ViolationWitnessWriter::visit(FunctionReturnEvent& event) {
    assert(inProgress && "Witness should be initialized before write and closed after write");
    createNode();
    openEdge();
    writeLocation(event.getLocation());
    mOS << "\t<data key=\"returnFromFunction\">" << event.getFunctionName() << "</data>\n";
    closeEdge();
}

void ViolationWitnessWriter::visit(FunctionCallEvent& event) {
    assert(inProgress && "Witness should be initialized before write and closed after write");
    ExprRef<AtomicExpr> retexpr = event.getReturnValue();
    
    // If it is a bitvector - we don't know the type -> output it in a hexadecimal form
    if(auto bv = llvm::dyn_cast<BvLiteralExpr>(retexpr)) {
        createNode();
        openEdge();
        writeLocation(event.getLocation());
        mOS << "\t<data key=\"assumption\">\\result==";
        
        unsigned int width = bv->getType().getWidth();
        
        llvm::SmallString<64> bits;
        bv->getValue().zextOrSelf(width).toString(bits, 16, false, false);

        mOS << "0x";
        mOS << bits;
        mOS << ";</data>\n";
        mOS << "\t<data key=\"assumption.resultfunction\">" << event.getFunctionName() << "</data>\n";
        closeEdge();
    } else if (!retexpr->getKind() == Expr::Undef) {
        createNode();
        openEdge();
        writeLocation(event.getLocation());
        mOS << "\t<data key=\"assumption\">\\result==";
        retexpr->print(mOS);
        mOS << ";</data>\n";
        mOS << "\t<data key=\"assumption.resultfunction\">" << event.getFunctionName() << "</data>\n";
        closeEdge();
    }
}

void ViolationWitnessWriter::visit(UndefinedBehaviorEvent& event) {
    assert(inProgress && "Witness should be initialized before write and closed after write");
    // writeLocation(event.getLocation());
    // mOS << "<data key=\"comment\">UndefBehaviorEvent visit</data>\n";
}

const std::string ViolationWitnessWriter::schema = R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<graphml xmlns="http://graphml.graphdrawing.org/xmlns" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
)";

const std::string ViolationWitnessWriter::keys = R"(
<key id="sourcecodelang" attr.name="sourcecodelang" for="graph"/>
<key id="witness-type" attr.name="witness-type" for="graph"/>
<key id="entry" attr.name="entry" for="node">
<default>false</default>
</key>
<key id="violation" attr.name="violation" for="node">
<default>false</default>
</key>

<key id="endline" attr.name="endline" for="edge"/>
<key id="enterFunction" attr.name="enterFunction" for="edge"/>
<key id="startline" attr.name="startline" for="edge"/>
<key id="returnFrom" attr.name="returnFrom" for="edge"/>
<key id="assumption" attr.name="assumption" for="edge"/>
<key id="control" attr.name="control" for="edge"/>
<key id="comment" attr.name="comment" for="edge"/>
<key attr.name="specification" attr.type="string" for="graph" id="specification"/>
<key attr.name="producer" attr.type="string" for="graph" id="producer"/>
<key attr.name="programFile" attr.type="string" for="graph" id="programfile"/>
<key attr.name="programHash" attr.type="string" for="graph" id="programhash"/>
<key attr.name="architecture" attr.type="string" for="graph" id="architecture"/>
<key attr.name="creationtime" attr.type="string" for="graph" id="creationtime"/>
)";

const std::string ViolationWitnessWriter::graph_data = R"(<graph edgedefault="directed">
<data key="witness-type">violation_witness</data>
<data key="producer">gazer-theta</data>
<data key="specification">CHECK( init(main()), LTL(G ! call(reach_error())) )</data>
<data key="sourcecodelang">C</data>
<data key="architecture">32bit</data>
)";

}