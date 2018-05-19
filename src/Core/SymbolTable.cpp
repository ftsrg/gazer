#include "gazer/Core/SymbolTable.h"

#include <fmt/format.h>

using namespace gazer;

SymbolAlreadyExistsError::SymbolAlreadyExistsError(std::string key)
    : logic_error(fmt::format(
        "Symbol '{0}' is already present in the symbol table.", key
    ))
{}

SymbolNotFoundError::SymbolNotFoundError(std::string key)
    : logic_error(fmt::format(
        "Symbol '{0}' was not found in the symbol table.", key
    ))
{}