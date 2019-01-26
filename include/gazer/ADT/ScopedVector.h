#ifndef _GAZER_CORE_SCOPEDVECTOR_H
#define _GAZER_CORE_SCOPEDVECTOR_H

#include <vector>
#include <stack>

namespace gazer
{

template<class ValueT>
class ScopedVector : private std::vector<ValueT>
{
public:
    void push() { mScopes.push(this->size()); }
    void pop()
    {
        size_t newSize = mScopes.top();
        this->resize(newSize);
        mScopes.pop();
    }


public:
    using typename std::vector<ValueT>::iterator;
    using typename std::vector<ValueT>::const_iterator;
    using typename std::vector<ValueT>::reverse_iterator;
    using typename std::vector<ValueT>::const_reverse_iterator;

    using std::vector<ValueT>::begin;
    using std::vector<ValueT>::end;
    using std::vector<ValueT>::rbegin;
    using std::vector<ValueT>::rend;
    using std::vector<ValueT>::push_back;
    using std::vector<ValueT>::emplace_back;

private:
    using std::vector<ValueT>::resize;

private:
    std::stack<size_t> mScopes;
};

}


#endif
