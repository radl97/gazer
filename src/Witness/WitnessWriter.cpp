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

#include <fstream>

using namespace gazer;

void WitnessWriter::createNode() {
    mOS << "\n<node id=\"N" << nodeID << "\">\n";
    if(nodeID==0) mOS << "\t<data key=\"entry\">true</data>\n";
    mOS << "</node>\n";
    nodeID++;
}

void WitnessWriter::createViolationNode() {
    mOS << "\n<node id=\"N" << nodeID << "\">\n";
    mOS << "\t<data key=\"violation\">true</data>\n";
    mOS << "</node>\n";
    nodeID++;
}

void WitnessWriter::openEdge() {
    assert(nodeID>0 && "Can't create witness edge, there is only one node");
    mOS << "\n<edge source=\"N" << nodeID-2 << "\" target=\"N" << nodeID-1 << "\">\n";
}

void WitnessWriter::closeEdge() {
    mOS << "</edge>\n";
}

void WitnessWriter::writeEdgeLocation(gazer::LocationInfo location) {
    if (location.getLine() != 0) {
        mOS << "\t<data key=\"startline\">" 
            << location.getLine()
            << "</data>\n"
            << "\t<data key=\"startoffset\">"
            << location.getColumn()
            << "</data>\n";
    }
}

void WitnessWriter::generateWitness(Trace& trace, llvm::DebugLoc failLocation) {
    nodeID = 0;
    mOS << keys << graph_data;
    mOS << "<data key=\"programfile\">" << failLocation->getFilename() << "</data>\n";

    createNode();
    write(trace);
    // creating violation node
    createViolationNode();
    openEdge();
    closeEdge();
    mOS << closing_brackets;
}

void WitnessWriter::visit(AssignTraceEvent& event) { // practically a new node and edge
    ExprRef<AtomicExpr> expr = event.getExpr();
    if (!llvm::dyn_cast<BvLiteralExpr>(expr)) {
        createNode(); // <node id="N[ID]"> (and entry, if first node) </node>
        openEdge(); // <edge source="Nlast" taerget="Nnext">
        writeEdgeLocation(event.getLocation());
        // TODO egyelőre mindig előbb kell nyitni nodeot és utána edget az egyszerű ID számolás miatt, de ezt lehetne fejleszteni
        // TODO helper functions: add assumption, add data, something like that?
        mOS << "\t<data key=\"assumption\">" << event.getVariable().getName() << "==";
        expr->print(mOS);
        mOS << ";</data>\n";
        closeEdge(); // </edge>
    }
}

// Megj. amúgy ilyen eventre még nem láttam példát a kimenetben
void WitnessWriter::visit(FunctionEntryEvent& event) { // practically a new node and edge
    createNode();
    openEdge();
    writeEdgeLocation(event.getLocation());
    mOS << "\t<data key=\"enterFunction\">" << event.getFunctionName() << "</data>\n";
    closeEdge();
}

// Megj. amúgy ilyen eventre még nem láttam példát a kimenetben
void WitnessWriter::visit(FunctionReturnEvent& event) { // practically a new node and edge
    createNode();
    openEdge();
    writeEdgeLocation(event.getLocation());
    mOS << "\t<data key=\"returnFromFunction\">" << event.getFunctionName() << "</data>\n";
    closeEdge();
}

void WitnessWriter::visit(FunctionCallEvent& event) { // assumption from result function
    createNode();
    openEdge();
    writeEdgeLocation(event.getLocation());
    mOS << "\t<data key=\"assumption\">\\result==";
    event.getReturnValue()->print(mOS);
    mOS << ";</data>\n";
    mOS << "\t<data key=\"assumption.resultfunction\">" << event.getFunctionName() << "</data>\n";
    closeEdge();
}

void WitnessWriter::visit(UndefinedBehaviorEvent& event) { // dunno yet what to do in graphml wit this
    writeEdgeLocation(event.getLocation());
    mOS << "UndefBehaviorEvent visit\n";
}

// TODO remove this hardcoded part 

const std::string WitnessWriter::graph_data = R"(
<graph edgedefault="directed">
<data key="sourcecodelang">C</data>
<data key="witness-type">violation_witness</data>
<data key="producer">Theta v1.3.0</data>
<data key="specification">CHECK( init(main()), LTL(G ! call(reach_error())) )</data>
<data key="architecture">32bit</data>
)";

const std::string WitnessWriter::keys = R"(
<?xml version="1.0" encoding="UTF-8"?>
<graphml xmlns="http://graphml.graphdrawing.org/xmlns/graphml"
xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"  
xsi:schemaLocation="http://graphml.graphdrawing.org/xmlns/graphml">
<key id="sourcecodelang" attr.name="sourcecodelang" for="graph"/>
<key id="witness-type" attr.name="witness-type" for="graph"/>
<key id="entry" attr.name="entry" for="node">
<default>false</default>
</key>
<key id="violation" attr.name="violation" for="node">
<default>false</default>
</key>
<key id="sink" attr.name="sink" for="node">
<default>false</default>
</key>

<key id="endline" attr.name="endline" for="edge"/>
<key id="enterFunction" attr.name="enterFunction" for="edge"/>
<key id="startline" attr.name="startline" for="edge"/>
<key id="returnFrom" attr.name="returnFrom" for="edge"/>
<key id="assumption" attr.name="assumption" for="edge"/>
<key id="control" attr.name="control" for="edge"/>
)";

const std::string WitnessWriter::closing_brackets = R"(
</graph>
</graphml>
)";