#ifndef _GAZER_LLVM_ANALYSIS_BMCTRACE_H
#define _GAZER_LLVM_ANALYSIS_BMCTRACE_H

namespace gazer
{

class BmcTraceEvent
{
public:
    enum EventKind
    {
        Assignment,
        FunctionEntry
    }
};

}

#endif
