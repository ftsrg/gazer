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
    Variable *getVariable(llvm::StringRef name);
    Variable *createVariable(std::string name, Type &type);

    void removeVariable(Variable* variable);

    void addManagedResouce(ManagedResource* resource);

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

/// Represents a resouce which is managed by a GazerContext object.
///
/// These objects are owned by their enclosing context and are destructed
/// automatically when their parent context dies.
class ManagedResource
{
protected:
    explicit ManagedResource(GazerContext& context)
        : mContext(context)
    {}

public:
    virtual ~ManagedResource() {}

protected:
    GazerContext& mContext;
};

} // end namespace gazer

#endif
