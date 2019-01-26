#ifndef _GAZER_ADT_SCOPEDMAP_H
#define _GAZER_ADT_SCOPEDMAP_H

#include <list>
#include <llvm/ADT/DenseMap.h>

namespace gazer
{

/// An associative container which stores its elements in scopes.
/// TODO: This is a crude implementation just for the API. An implementation rewrite will be needed.
template<class KeyT, class ValueT>
class ScopedMap
{
    using MapT = llvm::DenseMap<KeyT, ValueT>;
public:
    ScopedMap()
    {
        mData.emplace_back();
    }

    ValueT& operator[](const KeyT& key)
    {
        return mData.back()[key];
    }

    size_t count(const KeyT& key)
    {
        return mData.back().count(key);
    }

    size_t size()
    {
        size_t siz = 0;
        for (const MapT& map : mData) {
            siz += map.size();
        }

        return siz;
    }

    void push()
    {
        mData.emplace_back(mData.back().begin(), mData.back().end());
    }

    void pop()
    {
        mData.pop_back();
    }

private:
    std::list<MapT> mData;
};

}

#endif
