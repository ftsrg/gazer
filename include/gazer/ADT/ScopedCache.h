#ifndef GAZER_ADT_SCOPEDCACHE_H
#define GAZER_ADT_SCOPEDCACHE_H

#include <llvm/ADT/DenseMap.h>

#include <vector>

namespace gazer
{

/// A simple cache which stores its elements in scopes.
/// 
/// By default, the cache is constructed with one single scope,
/// called the root. If no other scopes are present in the internal stack,
/// all insertions will take place in the root scope.
/// The root scope cannot be pop'd out of the container, therefore clients
/// can always assume that there is a scope available for insertion.
template<
    class KeyT,
    class ValueT,
    class MapT = llvm::DenseMap<KeyT, ValueT>,
    class StorageT = std::vector<MapT>
>
class ScopedCache
{
public:
    using scope_iterator = typename MapT::iterator;
public:
    ScopedCache() {
        // Create the root scope.
        mStorage.emplace_back();
    }

    /// Inserts a new element with a given key into the current scope.
    void insert(const KeyT& key, ValueT value) {
        mStorage.back()[key] = value;
    }

    /// Returns an optional with the value corresponding to the given key.
    /// If a given key was not found in the current scope, this method will
    /// traverse all parent scopes. If the requested element was not found
    /// in any of the scopes, returns an empty optional.
    std::optional<ValueT> get(const KeyT& key) const {
        for (auto it = mStorage.rbegin(), ie = mStorage.rend(); it != ie; ++it) {
            auto result = it->find(key);
            if (result != it->end()) {
                return std::make_optional(result->second);
            }
        }

        return std::nullopt;
    }

    void clear() {
        mStorage.resize(1);
        mStorage.begin()->clear();
    }

    void push() {
        mStorage.emplace_back();
    }

    void pop() {
        assert(mStorage.size() > 1 && "Attempting to pop the root scope of a ScopedCache.");
        mStorage.pop_back();
    }

    scope_iterator current_begin() { return mStorage.back().begin(); }
    scope_iterator current_end() { return mStorage.back().end(); }

private:
    StorageT mStorage;
};

}

#endif
