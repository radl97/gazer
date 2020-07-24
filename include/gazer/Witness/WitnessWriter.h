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
#include <map>
#include <memory>

class Witness {
private:
    // TODO maybe Gazer Graph could be used instead another implementation?
    class WitnessNode {
        const bool entry, sink, violation; // only one entry allowed in a witness automaton!
        const int id;

    public:
        WitnessNode(int _id, bool _entry = false, bool _sink = false, bool _violation = false)
            : id(_id), entry(_entry), sink(_sink), violation(_violation) {}

        std::string toString() const;
    };

    class WitnessEdge {
        const int from_id, to_id; // "naiv" megoldás (& could be better - we'll see)
        const int id, startline, endline;
        // std::string sourcecode; // ez, vagy enter/return from function, vagy mivel?

    public:
        WitnessEdge(int _id, unsigned int _from_id, unsigned int _to_id, unsigned int _startline, unsigned int _endline)
            : id(_id), from_id(_from_id), to_id(_to_id),
            startline(_startline), endline(_endline) {}
        
        std::string toString() const;
    };

public:
    void generateWitnessFile() const;
    void createEntryNode(int _id, bool _sink, bool _violation);
    void createNode(int _id, bool _sink, bool _violation);
    void createEdge(int _id, unsigned int _from, unsigned int _to, unsigned int _startline, unsigned int _endline);

private:
    bool entryNodeExists = false;
    static const std::string keys;
    static const std::string graph_data;
    
    std::map<unsigned int, WitnessNode> nodes{}; // TODO id-t érdemes a node/edge objectben is tárolni, ha itt a map?
    std::map<unsigned int, WitnessEdge> edges{};
};

#endif