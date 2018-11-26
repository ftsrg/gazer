#ifndef _GAZER_CORE_SOLVER_SOLVER_H
#define _GAZER_CORE_SOLVER_SOLVER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Valuation.h"

namespace gazer
{

class Solver
{
public:
    enum SolverStatus {
        SAT, UNSAT, UNKNOWN
    };

    class InsertIterator
        : public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
    public:
        InsertIterator(Solver& solver)
            : mSolver(solver)
        {}
    public:
        InsertIterator& operator++() { return *this; }
        InsertIterator operator++(int) { return *this; }
        InsertIterator& operator*() { return *this; }

        InsertIterator& operator=(ExprPtr value) {
            mSolver.add(value);
            return *this;
        }
    private:
        Solver& mSolver;
    };
public:
    Solver(SymbolTable& symbols)
        : mSymbols(symbols)
    {}

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    template<class InputIterator>
    void add(InputIterator begin, InputIterator end) {
        for (InputIterator i = begin; i != end; ++i) {
            add(*i);
        }
    }

    void add(ExprPtr expr) {
        assert(expr->getType().isBoolType() && "Can only add bool expressions to a solver.");
        mStatCount++;
        addConstraint(expr);
    }

    unsigned getNumConstraints() const { return mStatCount; }

    virtual void dump(llvm::raw_ostream& os) = 0;
    virtual SolverStatus run() = 0;
    virtual Valuation getModel() = 0;

    virtual ~Solver() {}

protected:
    virtual void addConstraint(ExprPtr expr) = 0;

    SymbolTable& mSymbols;
private:
    unsigned mStatCount = 0;
};

class SolverFactory
{
public:
    /**
     * Creates a new solver instance with a given symbol table.
     */
    virtual std::unique_ptr<Solver> createSolver(SymbolTable& symbols) = 0;
};

/**
 * Exception class for solver-related errors.
 */
class SolverError : public std::runtime_error
{
public:
    SolverError(std::string message)
        : runtime_error(message)
    {}
};

}

#endif
