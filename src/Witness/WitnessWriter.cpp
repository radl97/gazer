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
#include "gazer/Witness/Witness.h"
#include <fstream>
#include <sstream>
#include <vector>

void Witness::createEntryNode(int _id, bool _sink, bool _violation) {
    if(entryNodeExists) throw new std::logic_error("More than one entry node created");
    entryNodeExists = true;
    this->nodes.emplace(std::make_pair(_id, WitnessNode{_id, true, _sink, _violation}));
}

void Witness::createNode(int _id, bool _sink, bool _violation) {
    this->nodes.emplace(std::make_pair(_id, WitnessNode{_id, false, _sink, _violation}));
}

void Witness::createEdge(int _id, unsigned int _from, unsigned int _to,
    unsigned int _startline, unsigned int _endline) {
    this->nodes.emplace(std::make_pair(_id, WitnessEdge{_id, _from, _to, _startline, _endline}));
}

std::string Witness::WitnessNode::toString() const {
    std::stringstream strstr{};
    strstr << "<node id=\"N" << id << "\">" << std::endl;
    if(entry) strstr << "<data key=\"entry\">true</data>" << std::endl;
    if(violation) strstr << "<data key=\"violation\">true</data>" << std::endl;
    if(sink) strstr << "<data key=\"sink\">true</data>" << std::endl;
    strstr << "</node>";
    return strstr.str();
}

std::string Witness::WitnessEdge::toString() const {
    std::stringstream strstr{};
    strstr << "<edge id=\"E" << id << "\" source=\"N" << from_id << "\" target=\"N" << to_id << "\">" << std::endl;
    strstr << "<data key=\"startline\">" << startline << "</data>" << std::endl;
    strstr << "<data key=\"endline\">" << endline << "</data>" << std::endl;
}

void Witness::generateWitnessFile() const {
    if(!entryNodeExists) throw new std::logic_error("No entry node in witness");

    std::ofstream witnessfile;
    witnessfile.open("example_witness.graphml");
    witnessfile << keys << graph_data;
    for(auto const & pair : nodes) {
        witnessfile << pair.second.toString();
    }
    for(auto const & pair : edges) {
        witnessfile << pair.second.toString();
    }
    witnessfile.close();
}

// TODO remove this hardcoded part 

const std::string Witness::graph_data = R"(
<graph edgedefault="directed">
<data key="sourcecodelang">C</data>
<data key="witness-type">violation_witness</data>
<data key="producer">Theta v1.3.0</data>
<data key="specification">CHECK( init(main()), LTL(G ! call(reach_error())) )</data>
)";

// <data key="programfile">minepump_spec1_product33_false-unreach-call_false-termination.cil.c</data>
// <data key="programhash">4988ed1a51716095b984ef9f31c0416bd8aad186</data>
// <data key="architecture">32bit</data>

const std::string Witness::keys = R"(
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
