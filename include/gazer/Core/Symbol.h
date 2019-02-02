#ifndef _GAZER_CORE_SYMBOL_H
#define _GAZER_CORE_SYMBOL_H

#include <llvm/ADT/StringMap.h>

namespace gazer
{

/// A uniquely-named element which lives in a potentially scoped context.
/// Symbols have two components: their fully qualified names (FQN) which must
/// must be unique within their GazerContext, and local names which must be unique
/// within their declaring scope.
///
/// The canonical way of writing FQNs is by using the notation "/scope1/scope2/varname".
class Symbol
{
protected:
    Symbol(std::string name)
        : mSymbolName(name)
    {}

public:
    /// Returns the local symbol name of this symbol.
    std::string getName() const { return mSymbolName; }

    /// Returns the fully qualified name of this symbol.
    llvm::Twine getFQN() const;

private:
    std::string mSymbolName;
};

/// Represents a scope in which all local symbol names are unique.
class SymbolScope
{
public:
    SymbolScope(std::string name);

    /// Returns the name of the scope. This name will be used in the FQNs.
    std::string getScopeName() const { return mScopeName; }
private:
    std::string mScopeName;
    llvm::StringMap<Symbol*> mSymbolNames;
};

}

#endif
