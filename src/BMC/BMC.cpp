#include "gazer/BMC/BMC.h"
#include "gazer/Core/Utils/CfaUtils.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/ADT/DenseMap.h>

#include <queue>
#include <iostream>
#include <deque>

using namespace gazer;

namespace
{

class BmcState
{
public:
    unsigned depth;
    Location* loc;
    BmcState* parent;
    CfaEdge* edge;
public:
    BmcState(unsigned depth, Location* location, BmcState* parent = nullptr, CfaEdge* edge = nullptr)
        : depth(depth), loc(location), parent(parent), edge(edge)
    {}
};

}

auto BoundedModelChecker::check(Automaton& cfa) -> Status
{
    unsigned depth = 0;
    std::vector<BmcState*> states;
    std::queue<BmcState*> queue;

    auto entry = new BmcState(0, &cfa.entry());
    states.push_back(entry);
    queue.push(entry);

    while (!queue.empty()) {
        auto state = queue.front();
        queue.pop();

        depth = state->depth;
        if (mCriterion(state->loc)) {
            // We found a criterion location.
            // Try to reconstruct the program path.
            std::vector<CfaEdge*> edges;

            std::cerr << "Found criterion location in depth " << state->depth << ".\n";

            auto parent = state;
            while (parent != entry) {
                edges.push_back(parent->edge);
                parent = parent->parent;
            }

            std::unique_ptr<Solver> solver = std::make_unique<Z3Solver>();
            std::cerr << "Transforming CFA path to SMT formulas.\n";
            PathToExprs(
                cfa.getSymbols(), edges.rbegin(), edges.rend(),
                Solver::InsertIterator(*solver)
            );
            std::cerr << "Running solver.\n";
            Solver::SolverStatus status = solver->run();
            if (status == Solver::SAT) {
                // We found a counterexample.           
                return Status::STATUS_UNSAFE;
            } else if (status == Solver::UNSAT) {
                // Continue checking.
                std::cerr << "Formula is UNSAT.\n";
            } else {
                throw std::logic_error("Invalid solver state.");
            }
        } else if (depth + 1 < mBound) {
            for (auto& edge : state->loc->outgoing()) {
                auto newState = new BmcState(depth + 1, &edge->getTarget(), state, edge);
                states.push_back(newState);
                queue.push(newState);
            }
        } else {
            // We reached the bound, do nothing.
        }
    }

    for (BmcState* state : states) {
        delete[] state;
    }

    return Status::STATUS_UNKNOWN;
}
