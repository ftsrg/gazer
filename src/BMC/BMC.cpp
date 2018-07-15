#include "gazer/BMC/BMC.h"
#include "gazer/Core/Utils/CfaUtils.h"
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Solver/Solver.h"

#include <llvm/ADT/DenseMap.h>

#include <queue>
#include <iostream>
#include <deque>

using namespace gazer;

#if 0
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

    void print(std::ostream& os) {
        os << "BmcState(loc=" << *loc << ", edge=";
        if (edge != nullptr) {
            os << *edge;
        }
        os << ", depth=" << depth << ")";
    }
};

}

auto BoundedModelChecker::check(Automaton& cfa) -> Status
{
    unsigned depth = 0;
    unsigned currentBound = 1;
    std::vector<std::unique_ptr<BmcState>> states;
    std::queue<BmcState*> queue;

    auto entry = states.emplace_back(new BmcState(0, &cfa.entry())).get();
    queue.push(entry);

    std::cerr << "Running BMC check...\n";

    while (!queue.empty()) {
        auto state = queue.front();
        queue.pop();
        
        //std::cerr << "Current state=";
        //state->print(std::cerr);
        //std::cerr << "\n";

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

            std::unique_ptr<Z3Solver> solver = std::make_unique<Z3Solver>();
            std::cerr << "Transforming CFA path to SMT formulas.\n";
            PathToExprs(
                cfa.getSymbols(), edges.rbegin(), edges.rend(),
                Solver::InsertIterator(*solver)
            );
            std::cerr << "Generated " << solver->getNumConstraints() << " formulas.\n";
            std::cerr << "Running solver.\n";
            Solver::SolverStatus status = solver->run();

            if (status == Solver::SAT) {
                // We found a counterexample.       
                std::cerr << "Formula is SAT. Model:\n";
                std::cerr << solver->getModel() << "\n";    
                return Status::STATUS_UNSAFE;
            } else if (status == Solver::UNSAT) {
                // Continue checking.
                std::cerr << "Formula is UNSAT.\n";
            } else {
                throw std::logic_error("Invalid solver state.");
            }
        } else if (depth + 1 < mBound) {
            for (auto& edge : state->loc->outgoing()) {
                //auto newState = new BmcState(depth + 1, &edge->getTarget(), state, edge);
                auto& newState = states.emplace_back(new BmcState(depth + 1, &edge->getTarget(), state, edge));
                queue.push(newState.get());
            }
        } else {
            // We reached the bound, do nothing.
        }
    }

    return Status::STATUS_UNKNOWN;
}
#endif
