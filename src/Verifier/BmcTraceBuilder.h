#ifndef _GAZER_SRC_VERIFIER_BMCTRACEBUILDER_H
#define _GAZER_SRC_VERIFIER_BMCTRACEBUILDER_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Valuation.h"
#include "gazer/ADT/ScopedCache.h"

#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>

#include <vector>
#include <string>
#include <gazer/Trace/Trace.h>

namespace llvm {
    class raw_ostream;
}

namespace gazer
{

class BmcTraceBuilder : public TraceBuilder
{
public:
    BmcTraceBuilder(
        GazerContext& context,
        const std::vector<Location*>& topo,
        const ScopedCache<Location*, std::pair<Variable*, ExprPtr>>& preds,
        Location* error
    ) :
        mContext(context),
        mTopo(topo), mPreds(preds),
        mError(error)
    {}

protected:
    std::vector<std::unique_ptr<TraceEvent>> buildEvents(Valuation& model) override;

private:

private:
    GazerContext& mContext;
    const const std::vector<Location*>& mTopo;
    const const ScopedCache<Location*, std::pair<Variable*, ExprPtr>>&  mPreds;
    Location* mError
};

} // end namespace gazer

#endif