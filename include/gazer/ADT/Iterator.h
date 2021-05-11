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

namespace gazer
{

template<class T>
typename std::unique_ptr<T>::pointer derefUniquePtr(std::unique_ptr<T>& ptr)
{
    return ptr.get();
}

template<class T>
const typename std::unique_ptr<T>::pointer derefUniquePtr(const std::unique_ptr<T>& ptr)
{
    return ptr.get();
}

template<
    class BaseIterator,
    class ReturnTy = decltype(std::declval<BaseIterator>()->get())>
class SmartPtrGetIterator : public llvm::iterator_adaptor_base<
    SmartPtrGetIterator<BaseIterator>,
    BaseIterator,
    typename std::iterator_traits<BaseIterator>::iterator_category
>
{
public:
    /*implicit*/ SmartPtrGetIterator(BaseIterator it)
        : SmartPtrGetIterator::iterator_adaptor_base(std::move(it))
    {}

    ReturnTy operator*() const { return this->wrapped()->get(); }
};

} // namespace gazer


#endif