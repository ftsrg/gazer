#ifndef GAZER_ADT_BIMAP_H
#define GAZER_ADT_BIMAP_H

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

/// A very simple bidirectional map for bijections.
template<
    class KeyT, class ValueT
>
class Bimap
{
public:

    void insert(const KeyT& key, V)

private:
    llvm::DenseMap<KeyT, ValueT> mLeft;
    llvm::DenseMap<ValueT, KeyT> mRight;
};

} // end namespace gazer

#endif
