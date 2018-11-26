#include "gazer/LLVM/BMC/BMC.h"
#include "gazer/Core/Utils/ExprUtils.h"
#include "gazer/Support/Stopwatch.h"

#include <llvm/IR/CFG.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace llvm;

cl::opt<bool> NoElimVars("no-elim-vars",
    cl::desc("Do not eliminate temporary variables"));

cl::opt<bool> AssumeNoNaN("assume-no-nan",
    cl::desc("Assume that floating-point operations never return NaN"));

cl::opt<bool> UseMathInt("use-math-int",
    cl::desc("Use mathematical integers instead of bitvectors."));

cl::opt<bool> DumpFormula("dump-formula",
    cl::desc("Dump the encoded program formula to stderr."));

cl::opt<bool> DumpSolverFormula("dump-solver-formula",
    cl::desc("Dump the formula in the solver's format"));

cl::opt<bool> DumpModel("dump-model",
    cl::desc("Dump the solver SAT model"));


static bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "gazer.error_code";
}

static bool isErrorBlock(const BasicBlock* bb)
{
    for (auto& instr : *bb) {
        if (instr.getOpcode() == llvm::Instruction::Call) {
            auto call = llvm::dyn_cast<llvm::CallInst>(&instr);
            llvm::Function* callee = call->getCalledFunction();

            if (isErrorFunctionName(callee->getName())) {
                return true;
            }
        }
    }

    return false;
}

static ExprPtr tryToEliminate(const llvm::Instruction& inst, ExprPtr expr)
{
    if (inst.getNumUses() != 1) {
        return nullptr;
    }

    // FCmp instructions will have multiple uses as an expression
    // for a single value, due to their NaN checks.
    // With -assume-no-nan this is no longer the case.
    if (!AssumeNoNaN && isa<FCmpInst>(*inst.user_begin())) {
        return nullptr;
    }

    auto nonNullary = dyn_cast<NonNullaryExpr>(expr.get());
    if (nonNullary == nullptr || nonNullary->getNumOperands() != 2) {
        return nullptr;
    }

    if (nonNullary->getKind() != Expr::Eq && nonNullary->getKind() != Expr::FEq) {
        return nullptr;
    }

    if (nonNullary->getOperand(0)->getKind() != Expr::VarRef) {
        return nullptr;
    }

    return nonNullary->getOperand(1);
}

BoundedModelChecker::BoundedModelChecker(
    llvm::Function& function,
    TopologicalSort& topo,
    ExprBuilder* exprBuilder,
    SolverFactory& solverFactory,
    llvm::raw_ostream& os
) :
    mFunction(function), mTopo(topo), mSolverFactory(solverFactory), mOS(os),
    mExprBuilder(exprBuilder),
    mIr2Expr(function, mSymbols, mExprBuilder, mVariables, mEliminatedVars)
{}

auto BoundedModelChecker::encode() -> ProgramEncodeMapT
{    
    size_t numBB = mFunction.getBasicBlockList().size();

    // Find blocks containing assertion failures
    llvm::SmallDenseMap<size_t, BasicBlock*, 1> errorBlocks;
    for (size_t i = 0; i < numBB; ++i) {
        if (isErrorBlock(mTopo[i])) {
            errorBlocks.insert({i, mTopo[i]});
        }
    }

    llvm::DenseMap<const BasicBlock*, size_t> blocks(numBB);
    for (size_t i = 0; i < numBB; ++i) {
        blocks.insert({mTopo[i], i});
    }

    // We are using a dynamic programming-based approach.
    // As the CFG is a DAG after unrolling, we can create a topoligcal sort
    // of its blocks. Then we create an array with the size of numBB, and
    // perform DP as the following:
    //  (0) dp[0] := True (as the entry node is always reachable)
    //  (1) dp[i] := Or(forall p in pred(i): And(dp[p], SMT(p,i)))
    // This way dp[err] will contain the SMT encoding of all bounded error paths.
    std::vector<ExprPtr> dp(numBB, BoolLiteralExpr::getFalse());
    ProgramEncodeMapT result;

    // The entry block is always reachable
    dp[0] = BoolLiteralExpr::getTrue();

    for (size_t i = 1; i < numBB; ++i) {
        BasicBlock* bb = mTopo[i];
        llvm::SmallVector<ExprPtr, 2> exprs;

        for (BasicBlock* pred : llvm::predecessors(bb)) {
            auto predIt = blocks.find(pred);
            assert(predIt != blocks.end()
                && "All blocks must be present in the block map");

            size_t predIdx = blocks[pred];
            assert(predIdx < i
                && "Predecessors must be before block in a topological sort");

            ExprPtr predFormula = dp[predIdx];
            if (predFormula != BoolLiteralExpr::getFalse()) {
                auto andFormula = mExprBuilder->And(
                    predFormula,
                    encodeEdge(pred, bb)
                );
                exprs.push_back(andFormula);
            }
        }

        if (!exprs.empty()) {
            auto expr = mExprBuilder->Or(exprs.begin(), exprs.end());
            dp[i] = expr;
        }
    }

    for (auto& entry : errorBlocks) {
        size_t idx = entry.first;
        BasicBlock* block  = entry.second;
        ExprPtr expr = dp[idx];

        if (AssumeNoNaN) {
            // We must also assume that no float type is NaN
            ExprVector notNanExprs;

            for (auto& var : mSymbols) {
                if (var.second->getType().isFloatType()) {
                    notNanExprs.push_back(mExprBuilder->Not(
                        mExprBuilder->FIsNan(var.second->getRefExpr())
                    ));
                }
            }

            expr = mExprBuilder->And(
                expr,
                mExprBuilder->And(notNanExprs.begin(), notNanExprs.end())
            );
        }

        result[block] = expr;
    }

    return result;
}

ExprPtr BoundedModelChecker::encodeEdge(BasicBlock* from, BasicBlock* to)
{
    ExprVector exprs;
    auto terminator = from->getTerminator();
    
    // Find out which branch we are taking
    size_t succIdx = 0;
    while (succIdx != terminator->getNumSuccessors()) {
        if (terminator->getSuccessor(succIdx) == to) {
            break;
        }

        succIdx++;
    }

    assert(succIdx != terminator->getNumSuccessors()
        && "'From' should be a parent of 'to'");

    auto cacheResult = mFormulaCache.find(from);
    if (cacheResult == mFormulaCache.end()) {
        std::vector<ExprPtr> fromExprs;

        // We are starting from the non-phi nodes at the 'from' part of the block
        for (auto it = from->getFirstInsertionPt(); it != from->end(); ++it) {
            if (it->isTerminator()) {
                // Avoid handling terminators
                continue;
            }

            auto expr = mIr2Expr.transform(*it);

            if (!NoElimVars) {                
                if (auto elimExpr = tryToEliminate(*it, expr)) {
                    mEliminatedVars[&*it] = elimExpr;
                    continue;
                }
            }
            
            fromExprs.push_back(expr);
        }

        ExprPtr fromExpr;
        if (fromExprs.size() >= 1) {
            fromExpr = mIr2Expr.getBuilder()->And(fromExprs.begin(), fromExprs.end());
        } else {
            fromExpr = mIr2Expr.getBuilder()->True();
        }            
        mFormulaCache[from] = fromExpr;
        exprs.push_back(fromExpr);
    } else {
        exprs.push_back(cacheResult->second);
    }

    // Handle the branch
    exprs.push_back(mIr2Expr.transform(*from->getTerminator(), succIdx));

    // Handle the PHI nodes
    for (auto it = to->begin(); it != to->getFirstInsertionPt(); ++it) {
        auto expr = mIr2Expr.transform(*it, succIdx, from);
        exprs.push_back(expr);
    }

    return mIr2Expr.getBuilder()->And(exprs.begin(), exprs.end());
}

std::unique_ptr<SafetyResult> BoundedModelChecker::run()
{
    llvm::DenseMap<const Variable*, llvm::Value*> variableToValueMap;

    mOS << "Program size: \n";
    mOS << "   Blocks: " << mFunction.getBasicBlockList().size() << "\n";
    mOS << "Encoding program into SMT formula.\n";

    auto& context = mFunction.getContext();

    llvm::DenseMap<BasicBlock*, size_t> blocks(mTopo.size());
    llvm::DenseMap<BasicBlock*, llvm::Value*> preds;

    for (size_t i = 0; i < mTopo.size(); ++i) {
        blocks.insert({mTopo[i], i});
    }

    // Create predecessor identifications
    llvm::Type* predTy = llvm::IntegerType::get(context, 32);
    for (BasicBlock& bb : mFunction) {
        size_t bbID = blocks[&bb];

        std::string name = "pred" + std::to_string(bbID);
        auto phi = llvm::PHINode::Create(predTy, 0, name);

        for (BasicBlock* pred : llvm::predecessors(&bb)) {
            size_t predID = blocks[pred];
            phi->addIncoming(
                llvm::ConstantInt::get(
                    predTy,
                    llvm::APInt(predTy->getIntegerBitWidth(), predID)
                ),
                pred
            );
        }

        if (phi->getNumIncomingValues() > 1) {
            bb.getInstList().push_front(phi);
            preds[&bb] = phi;
            // Also insert this into the symbol table
            auto& variable = mSymbols.create(name, gazer::BvType::get(32));
            mVariables[phi] = &variable;
        } else {
            // Do not create a PHI node with only one argument
            if (phi->getNumIncomingValues() == 1) {
                preds[&bb] = phi->getIncomingValue(0);
            }

            phi->dropAllReferences();
            phi->deleteValue();
        }
    }

    ProgramEncodeMapT result = this->encode();

    for (auto& entry : result) {
        llvm::BasicBlock* errorBlock = entry.first;
        ExprPtr formula = entry.second;

        mOS << "Checking for error block '";
        errorBlock->printAsOperand(mOS);
        mOS << "'\n";

        std::unique_ptr<Solver> solver = mSolverFactory.createSolver(mSymbols);

        mOS << "   Transforming formula.\n";

        try {
            //formula->print(mOS);
            if (DumpFormula) {
                FormatPrintExpr(formula, mOS);
                mOS << "\n";
            }
            solver->add(formula);

            if (DumpSolverFormula) {
                solver->dump(mOS);
            }

            mOS << "   Running solver.\n";
            auto status = solver->run();

            if (status == Solver::SAT) {
                mOS << "   Formula is SAT\n";
                Valuation model = solver->getModel();

                if (DumpModel) {
                    model.print(mOS);
                }

                // Display a counterexample trace
                LLVMBmcTraceBuilder builder(
                    mTopo, blocks, preds, mIr2Expr.getVariableMap(),
                    errorBlock
                );

                auto trace = builder.build(model);

/*
                auto trace = BmcTrace::Create(
                    mTopo,
                    blocks,
                    preds,
                    errorBlock,
                    model,
                    mIr2Expr.getVariableMap()
                ); */

                // Find the error code
                bool foundEC = false;
                unsigned ec;
                for (llvm::Instruction& inst : *errorBlock) {
                    if (auto call = dyn_cast<CallInst>(&inst)) {
                        auto callee = call->getCalledFunction();
                        assert(callee
                            && "Indirect calls are not allowed in error blocks.");
                        if (callee->getName() == "gazer.error_code") {
                            foundEC = true;
                            auto codeVal = call->getArgOperand(0);
                            if (auto ci = dyn_cast<ConstantInt>(codeVal)) {
                                ec = ci->getLimitedValue();
                            } else {
                                // It should be in a variable, retrieve it from the model
                                auto ecVar = mIr2Expr.getVariableMap().find(codeVal);
                                assert(ecVar != mIr2Expr.getVariableMap().end());
                                
                                auto ecExpr = model[ecVar->second];

                                ec = (cast<BvLiteralExpr>(ecExpr.get()))
                                    ->getValue()
                                    .getLimitedValue();
                            }
                            break;
                        }
                    }
                }

                assert(foundEC
                    && "Error code intrinsic should be available");

                return SafetyResult::CreateFail(ec, std::move(trace));
            } else if (status == Solver::UNSAT) {
                mOS << "   Formula is UNSAT\n";
            } else {
                mOS << "   Unknown solver state.";
            }
        } catch (SolverError& e) {
            mOS << e.what() << "\n";
        }
    }

    return SafetyResult::CreateSuccess();
}
