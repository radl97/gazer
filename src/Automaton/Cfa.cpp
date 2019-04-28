#include <gazer/Core/LiteralExpr.h>
#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/StringExtras.h>
#include <boost/iterator/indirect_iterator.hpp>

using namespace gazer;

Cfa::Cfa(GazerContext &context, std::string name, AutomataSystem* parent)
    : mName(name), mContext(context)
{
    // Create an entry and exit location
    Location* entry = this->createLocation();
    Location* exit = this->createLocation();

    mEntry = entry;
    mExit = exit;
}

// Location and edge construction
//-------------------------------------------------------------------------
Location *Cfa::createLocation()
{
    Location* loc = new Location(mLocationIdx++);
    mLocations.emplace_back(loc);

    return loc;
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, ExprPtr guard,
        std::vector<VariableAssignment> assignments)
{
    auto edge = new AssignTransition(source, target, guard, assignments);
    mTransitions.emplace_back(edge);
    source->addOutgoing(edge);
    target->addIncoming(edge);

    return edge;
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target) {
    return createAssignTransition(source, target, BoolLiteralExpr::True(mContext), {});
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, std::vector<VariableAssignment> assignments)
{
    return createAssignTransition(source, target, BoolLiteralExpr::True(mContext), assignments);
}

AssignTransition *Cfa::createAssignTransition(Location *source, Location *target, ExprPtr guard)
{
    return createAssignTransition(source, target, guard, {});
}

CallTransition *Cfa::createCallTransition(Location *source, Location *target, ExprPtr guard,
    Cfa *callee, std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs)
{
    assert(source != nullptr);
    assert(target != nullptr);

    auto call = new CallTransition(source, target, guard, callee, inputArgs, outputArgs);
    mTransitions.emplace_back(call);
    source->addOutgoing(call);
    target->addIncoming(call);

    return call;
}

CallTransition *Cfa::createCallTransition(
    Location *source,
    Location *target,
    Cfa *callee,
    std::vector<VariableAssignment> inputArgs,
    std::vector<VariableAssignment> outputArgs)
{
    return createCallTransition(source, target, BoolLiteralExpr::True(mContext), callee, inputArgs, outputArgs);
}

// Member variables
//-----------------------------------------------------------------------------

Variable *Cfa::addInput(llvm::StringRef name, Type &type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mInputs.push_back(variable);

    return variable;
}

Variable *Cfa::addOutput(llvm::StringRef name, Type &type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mOutputs.push_back(variable);

    return variable;
}

Variable *Cfa::addLocal(llvm::StringRef name, Type &type)
{
    Variable* variable = this->createMemberVariable(name, type);
    mLocals.push_back(variable);

    return variable;
}

Variable *Cfa::createMemberVariable(llvm::Twine name, Type &type)
{
    llvm::Twine variableName = mName + "/" + name;

    return mContext.createVariable(variableName.str(), type);
}

void Cfa::addNestedAutomaton(Cfa* cfa)
{
    assert(cfa != nullptr);
    assert(cfa->mParentAutomaton == nullptr);

    mNestedAutomata.push_back(cfa);
    cfa->mParentAutomaton = this;
}

// Support code for locations
//-----------------------------------------------------------------------------

void Location::addIncoming(Transition *edge)
{
    assert(edge->getTarget() == this);
    mIncoming.push_back(edge);
}

void Location::addOutgoing(Transition *edge)
{
    assert(edge->getSource() == this);
    mOutgoing.push_back(edge);
}

void Location::removeIncoming(Transition *edge)
{
    mIncoming.erase(std::remove(mIncoming.begin(), mIncoming.end(), edge));
}

void Location::removeOutgoing(Transition *edge)
{
    mOutgoing.erase(std::remove(mOutgoing.begin(), mOutgoing.end(), edge));
}

// Transitions
//-----------------------------------------------------------------------------
AssignTransition::AssignTransition(
    Location *source, Location *target, ExprPtr guard,
    std::vector<VariableAssignment> assignments
) : Transition(source, target, guard, Transition::Edge_Assign), mAssignments(assignments)
{}

CallTransition::CallTransition(
    Location *source, Location *target, ExprPtr guard, Cfa *callee,
    std::vector<VariableAssignment> inputArgs, std::vector<VariableAssignment> outputArgs
) : Transition(source, target, guard, Transition::Edge_Call), mCallee(callee),
    mInputArgs(inputArgs), mOutputArgs(outputArgs)
{
    assert(callee != nullptr);
}

// Automata system
//-----------------------------------------------------------------------------

AutomataSystem::AutomataSystem(GazerContext &context)
    : mContext(context)
{}

Cfa *AutomataSystem::createCfa(std::string name)
{
    Cfa* cfa = new Cfa(mContext, name, this);
    mAutomata.emplace_back(cfa);

    return cfa;
}

Cfa* AutomataSystem::createNestedCfa(Cfa* parent, std::string name)
{
    auto fullName = parent->getName() + "/" + name;
    Cfa* cfa = new Cfa(mContext, fullName.str(), this);
    mAutomata.emplace_back(cfa);

    parent->addNestedAutomaton(cfa);

    return cfa;
}

void AutomataSystem::addGlobalVariable(Variable* variable)
{
    mGlobalVariables.push_back(variable);
}
