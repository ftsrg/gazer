/// \brief This file defines GazerContext, a container for all unique types,
/// variables and expressions.
#ifndef _GAZER_CORE_GAZERCONTEXT_H
#define _GAZER_CORE_GAZERCONTEXT_H

#include <llvm/ADT/StringRef.h>

namespace gazer
{

class Type;
class Variable;
class GazerContext;
class GazerContextImpl;
class ManagedResource;

class GazerContext
{
public:
    GazerContext();

    GazerContext(const GazerContext&) = delete;
    GazerContext& operator=(const GazerContext&) = delete;

    ~GazerContext();

public:

    /// Declares a new variable with the given name and type,
    /// or the already declared instance if a variable already
    /// exists with the given name.
    Variable* variableDecl(llvm::StringRef name, Type& type);

    Variable *getVariable(llvm::StringRef name);
    Variable *createVariable(std::string name, Type &type);

    void removeVariable(Variable* variable);

    void dumpStats(llvm::raw_ostream& os) const;

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
