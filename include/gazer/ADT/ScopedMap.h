#ifndef _GAZER_ADT_SCOPEDMAP_H
#define _GAZER_ADT_SCOPEDMAP_H

#include <list>
#include <llvm/ADT/DenseMap.h>

#if 0
namespace gazer
{

/// An associative container which stores its elements in scopes.
/// 
/// By default, the container is constructed with one single scope,
/// called the root. If no other scopes are present in the internal stack,
/// all insertions will take place in the root scope.
/// The root scope cannot be pop'd out of the container, therefore clients
/// can always assume that there is a scope available for insertion.
template<
    class KeyT,
    class ValueT,
    class MapT = llvm::DenseMap<KeyT, ValueT>,
    class ContainerT = std::list<MapT>
>
class ScopedMap
{
    struct Scope
    {
        MapT data;
        Scope* parent;
    };

    class ScopedMapIterator
    {
    public:
        ScopedMapIterator(
            ScopedMap& parent,
            ContainerT::iterator scope,
            MapT::iterator pos
        ) : mParentMap(parent), mCurrentScope(scope), mMapIterator(pos)
        {}

        ScopedMapIterator& operator++() { // preincrement
            if (mCurrentScope == mParentMap.mData.rend()) {
                // We are in the root scope.
                if (mMapIterator == mCurrentScope.end()) {
                    // If the scope iterator is set to be before the first element,
                    // and the map iterator is after its last, we are at the end of
                    // the root scope. Set the scope iterator to the first scope,
                    // and set the map iterator to its first element.
                    mCurrentScope = mParentMap.mData.begin();
                    mMapIterator = mCurrentScope->begin();
                } else {
                    mMapIterator
                }



                return *this;
            }

            // We reached the end of our current scope.
            if (mMapIterator == mCurrentScope.end()) {
                ++mCurrentScope;
                mMapIterator = mCurrentScope->begin();
            }
        }
    private:
        ScopedMap& mParentMap;
        ContainerT::iterator mCurrentScope;
        MapT::iterator mMapIterator;
    };
public:
    using key_type = KeyT;
    using mapped_type = ValueT;
    using size_type = std::size_t;
    using value_type = std::pair<const KeyT, ValueT>;

    using iterator = ScopedMapIterator;
public:
    void swap(ScopedMap& other);

    ValueT& operator[](const KeyT& key);
    ValueT& operator[](KeyT&& key);

    void push() {
        mData.emplace_back();
    }

    void pop() {
        mData.pop_back();
    }

    iterator begin() { return ScopedMapIterator(*this, mData.rend(), mRootScope.begin()); }
    iterator end() { return ScopedMapIterator(*this, mData.end(), mRootScope.end()); }

private:
    iterator getElement(KeyT&& key) {
        for (auto it = mData.rbegin(); it != mData.rend(); ++it) {
            auto result = it->find(key);
            if (result != it->end()) {

            }
        }
    }

private:
    MapT mRootScope;
    ContainerT mData;
};

}
#endif

#endif
