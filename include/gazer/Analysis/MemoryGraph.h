#ifndef GAZER_ANALYSIS_MEMORYGRAPH_H
#define GAZER_ANALYSIS_MEMORYGRAPH_H

#include <llvm/IR/Instruction.h>

namespace gazer
{

using MemoryObjectSize = uint64_t;
constexpr MemoryObjectSize InvalidMemoryObjectSize = ~uint64_t(0);


}

#endif
