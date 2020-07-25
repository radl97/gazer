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

#include "gazer/Trace/TraceWriter.h"
#include "gazer/Trace/Trace.h"

#include <string>
#include <memory>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

class WitnessWriter : TraceWriter {
public:
    explicit WitnessWriter(llvm::raw_ostream& os) : TraceWriter(os) {}
    void write(Trace& trace) override;

private:
    void visit(AssignTraceEvent& event) override;
    void visit(FunctionEntryEvent& event) override;
    void visit(FunctionReturnEvent& event) override;
    void visit(FunctionCallEvent& event) override;
    void visit(UndefinedBehaviorEvent& event) override;

    unsigned int nodeID = 0;
    static const std::string keys;
    static const std::string graph_data;
    static const std::string closing_brackets;

    void createNode();

    void openEdge();
    void closeEdge();

    void writeLocation(gazer::LocationInfo location);
};

#endif