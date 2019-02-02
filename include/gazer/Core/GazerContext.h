/// \brief This file defines GazerContext, a container for all unique types,
/// variables and expressions.
#ifndef _GAZER_CORE_GAZERCONTEXT_H
#define _GAZER_CORE_GAZERCONTEXT_H

#include "gazer/Core/Type.h"

#include <llvm/ADT/StringRef.h>

namespace gazer
{

class Variable;
class GazerContextImpl;

class GazerContext
{
public:
    GazerContext();

    GazerContext(const GazerContext&) = delete;
    GazerContext& operator=(const GazerContext&) = delete;

    ~GazerContext();

public:
    Variable *getVariable(llvm::StringRef name);

    Variable *createVariable(std::string name, Type &type);

public:
    const std::unique_ptr<GazerContextImpl> pImpl;
};

inline bool operator==(const GazerContext& lhs, const GazerContext& rhs) {
    // We only consider two context objects equal if they are the same object.
    return &lhs == &rhs;
}
inline bool operator!=(const GazerContext& lhs, const GazerContext& rhs) {
    return !(lhs == rhs);
}

} // end namespace gazer

#endif
