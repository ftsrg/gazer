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
#include <ctime>

using namespace gazer;

std::string CorrectnessWitnessWriter::SourceFileName{};

const std::string nodes = R"(<node id="N0">
<data key="entry">true</data>
</node>
)";

const std::string schema = R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<graphml xmlns="http://graphml.graphdrawing.org/xmlns" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
)";

const std::string keys = R"(
<key id="sourcecodelang" attr.name="sourcecodelang" for="graph"/>
<key id="witness-type" attr.name="witness-type" for="graph"/>
<key id="entry" attr.name="entry" for="node">
<default>false</default>
</key>
<key id="invariant" attr.name="invariant" for="node">
<default>false</default>
</key>
<key attr.name="specification" attr.type="string" for="graph" id="specification"/>
<key attr.name="producer" attr.type="string" for="graph" id="producer"/>
<key attr.name="programFile" attr.type="string" for="graph" id="programfile"/>
<key attr.name="programHash" attr.type="string" for="graph" id="programhash"/>
<key attr.name="architecture" attr.type="string" for="graph" id="architecture"/>
<key attr.name="creationtime" attr.type="string" for="graph" id="creationtime"/>
)";

const std::string graph_data = R"(<graph edgedefault="directed">
<data key="witness-type">correctness_witness</data>
<data key="producer">gazer-theta</data>
<data key="specification">CHECK( init(main()), LTL(G ! call(reach_error())) )</data>
<data key="sourcecodelang">C</data>
<data key="architecture">32bit</data>
)";

void CorrectnessWitnessWriter::outputWitness()
{
    time_t rawtime;
    struct tm * ptm;
    time ( &rawtime );
    ptm = gmtime( &rawtime ); // UTC timestamp
    std::stringstream timestamp;
    timestamp << std::put_time(ptm, "%FT%T");

    mOS << schema;
    mOS << keys;
    mOS << graph_data;
    mOS << "<data key=\"programhash\">" << mHash << "</data>";
    mOS << "<data key=\"creationtime\">" << timestamp.str() << "</data>\n";
    mOS << "<data key=\"programfile\">" << SourceFileName << "</data>\n";
    mOS << nodes;
    mOS << "</graph>";
    mOS << "</graphml>";
}