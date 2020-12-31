//==-------------------------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_ADT_ITERATOR_H
#define GAZER_ADT_ITERATOR_H

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/Casting.h>

namespace gazer
{

/// An iterator adaptor that filters a range of polymorphic objects that use LLVM-style RTTI.
/// On any given range, the iterator skips all elements that do not have the runtime type \p T
/// (i.e. elements on which `llvm::isa<T>(elem)` is false). Retained elements are mapped to their
/// proper type using `llvm::cast<T>`. This allows to simplify code like:
///     for (llvm::Instruction& inst : llvm::instructions(function)) {
///         if (auto* call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
///             ...
///         }
///     }
/// into the following:
///     for (llvm::CallInst& call : classof_range<llvm::CallInst>(llvm::instructions(function)) {
///         ...
///     }
/// .
/// Note that the returned type can be a raw pointer or reference extracted from a smart pointer,
/// which can lead to memory errors if the resulting value is incorrectly stored somewhere.
template<typename T, typename WrappedIteratorType, typename ReturnTy = decltype(llvm::cast<T>(*std::declval<WrappedIteratorType>()))>
class classof_iterator :
    public llvm::iterator_adaptor_base<
        classof_iterator<T, WrappedIteratorType, ReturnTy>,
        WrappedIteratorType,
        typename std::iterator_traits<WrappedIteratorType>::iterator_category,
        std::remove_reference_t<ReturnTy>
    >
{
    using BaseT = llvm::iterator_adaptor_base<
        classof_iterator<T, WrappedIteratorType, ReturnTy>,
        WrappedIteratorType,
        typename std::iterator_traits<WrappedIteratorType>::iterator_category,
        std::remove_reference_t<ReturnTy>
    >;

public:
    classof_iterator(WrappedIteratorType begin, WrappedIteratorType end)
        : BaseT(begin), mEnd(end)
    {
        this->findNextValid();
    }

    classof_iterator<T, WrappedIteratorType, ReturnTy>& operator++()
    {
        BaseT::operator++();
        this->findNextValid();
        return *this;
    }

    ReturnTy operator*() const
    {
        return llvm::cast<T>(*this->I);
    }

private:
    void findNextValid() {
        while (this->I != mEnd && !llvm::isa<T>(*this->I)) {
            BaseT::operator++();
        }
    }

    WrappedIteratorType mEnd;
};

template<typename T, typename WrappedIteratorType>
auto classof_range(WrappedIteratorType first, WrappedIteratorType last)
    -> llvm::iterator_range<classof_iterator<T, WrappedIteratorType>>
{
    classof_iterator<T, WrappedIteratorType> begin(first, last);
    classof_iterator<T, WrappedIteratorType> end(last, last);

    return llvm::make_range(begin, end);
}

template<typename T, typename Range, typename WrappedIteratorType = decltype(std::begin(std::declval<Range>()))>
auto classof_range(Range&& range)
    -> llvm::iterator_range<classof_iterator<T, WrappedIteratorType>>
{
    return classof_range<T>(std::begin(range), std::end(range));
}

/// A simple iterator adaptor that simplifies a smart pointer into a raw pointer.
/// Note that this class is mostly present to work around some implementation difficulties
/// in our Graph structure. Pointers returned by this iterator should never be stored.
template<
    class BaseIterator,
    class ReturnTy = decltype(std::declval<BaseIterator>()->get())>
class SmartPtrGetIterator : public llvm::iterator_adaptor_base<
    SmartPtrGetIterator<BaseIterator>,
    BaseIterator,
    typename std::iterator_traits<BaseIterator>::iterator_category,
    ReturnTy
>
{
public:
    explicit SmartPtrGetIterator(BaseIterator it)
        : SmartPtrGetIterator::iterator_adaptor_base(std::move(it))
    {}

    ReturnTy operator*() const { return this->wrapped()->get(); }
};

} // namespace gazer


#endif