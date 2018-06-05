#ifndef _GAZER_CORE_SYMBOLTABLE_H
#define _GAZER_CORE_SYMBOLTABLE_H

#include "gazer/Core/Variable.h"

#include <llvm/ADT/StringMap.h>

namespace gazer
{

struct SymbolAlreadyExistsError : std::logic_error {
    SymbolAlreadyExistsError(std::string key);
};

struct SymbolNotFoundError : std::logic_error {
    SymbolNotFoundError(std::string key);
};

/**
 * This class represents a symbol table implementation.
 * The symbol table constructs and owns all of its stored entries
 * through unique pointers.
 */
template<class ValueT>
class SymbolTableBase
{
    using VariableMapT = llvm::StringMap<std::unique_ptr<ValueT>>;
public:
    SymbolTableBase() = default;

    SymbolTableBase(const SymbolTableBase&) = delete;
    SymbolTableBase& operator=(const SymbolTableBase&) = delete;

    /**
     * Constructs a variable and returns a reference to it.
     */
    template<typename... Args>
    ValueT& create(std::string name, Args&&... args)
    {
        // Try to construct this variable inside the container
        auto result = mVariableMap.try_emplace(name, std::make_unique<ValueT>(args...));

        if (!result.second) {
            // We don't allow duplicate symbols
            throw SymbolAlreadyExistsError(name);
        }

        // Here result.first is an iterator to the inserted entry.
        // The inserted value also needs to be queried from this iterator.
        return *((result.first)->second);
    }

    /**
     * Queries the symbol table for a given symbol name.
     * Throws std::logic_error if no symbol exists with the given name.
     */
    ValueT& operator[](const std::string& name)
    {
        auto result = mVariableMap.find(name);
        if (result == mVariableMap.end()) {
            throw SymbolNotFoundError(name);
        }

        return *(result->second);
    }

    /**
     * Queries the symbol table for a given symbol name
     * and returns a optional.
     */
    std::optional<std::reference_wrapper<ValueT>> get(const std::string& name)
    {
        auto result = mVariableMap.find(name);
        if (result == mVariableMap.end()) {
            return std::nullopt;
        }

        return std::optional<std::reference_wrapper<ValueT>>(*result->second);
    }

    using iterator = typename VariableMapT::iterator;

    iterator begin() { return mVariableMap.begin(); }
    iterator end()   { return mVariableMap.end(); }

    size_t size() const { return mVariableMap.size(); }

private:
    VariableMapT mVariableMap;
};

class SymbolTable : private SymbolTableBase<Variable>
{
public:
    using SymbolTableBase::get;
    using SymbolTableBase::operator[];
    using SymbolTableBase::SymbolTableBase;
    using SymbolTableBase::size;

    using SymbolTableBase::iterator;
    using SymbolTableBase::begin;
    using SymbolTableBase::end;

    Variable& create(std::string name, const Type* type) {
        return SymbolTableBase::create(name, name, *type);
    }
    
    Variable& create(std::string name, const Type& type) {
        return SymbolTableBase::create(name, name, type);
    }
};

}

#endif
