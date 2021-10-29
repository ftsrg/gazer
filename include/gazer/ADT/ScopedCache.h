//==- ScopedCache.h ---------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_ADT_SCOPEDCACHE_H
#define GAZER_ADT_SCOPEDCACHE_H

#include <llvm/ADT/DenseMap.h>

#include <vector>
#include <optional>

namespace gazer
{

/// An associative cache which stores its elements in scopes.
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
    using iterator = typename MapT::iterator;
    using scope_iterator = typename StorageT::iterator;
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

    iterator current_begin() { return mStorage.back().begin(); }
    iterator current_end() { return mStorage.back().end(); }

    scope_iterator scope_begin() { return mStorage.begin(); }
    scope_iterator scope_end() { return mStorage.end(); }
    llvm::iterator_range<scope_iterator> scopes() { return llvm::make_range(scope_begin(), scope_end()); }

private:
    StorageT mStorage;
};

}

#endif
