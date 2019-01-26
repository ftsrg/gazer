/// \brief This file defines GazerContext, a container for all unique types,
/// variables and expressions. These objects are considered alive as long as
/// the GazerContext object defining them is alive.
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
    ~GazerContext();

public:
    Variable* getVariable(llvm::StringRef name);
    Variable* createVariable(std::string name, Type& type);

public:
    const std::unique_ptr<GazerContextImpl> pImpl;
};

} // end namespace gazer

#endif
