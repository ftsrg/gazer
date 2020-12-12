//==- StringUtils.h ---------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_ADT_STRINGUTILS_H
#define GAZER_ADT_STRINGUTILS_H

#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/raw_ostream.h>

namespace gazer
{

template<class ValueT>
std::string value_to_string(const ValueT& value)
{
    std::string buffer;
    llvm::raw_string_ostream rso(buffer);

    rso << value;
    rso.flush();

    return rso.str();
}

template<
    class Range,
    class Iterator = decltype(std::declval<Range>().begin()),
    class ValueT = decltype(*std::declval<Iterator>()),
    class ResultIterator = llvm::mapped_iterator<Iterator, std::function<std::string(ValueT&)>>
>
llvm::iterator_range<ResultIterator> to_string_range(Range&& range)
{
    std::function<std::string(ValueT&)> toString = [](ValueT& value) -> std::string {
        std::string buffer;
        llvm::raw_string_ostream rso(buffer);

        rso << value;
        rso.flush();

        return rso.str();
    };

    auto begin = llvm::map_iterator(range.begin(), toString);
    auto end = llvm::map_iterator(range.end(), toString);

    return llvm::make_range(begin, end);
}

template<
    class StreamT,
    class Iterator
>
void join_print(StreamT& os, Iterator begin, Iterator end, llvm::StringRef separator)
{
    if (begin == end) {
        return;
    }

    os << *begin;

    while (++begin != end) {
        os << separator;
        os << (*begin);
    }
}

template<class StreamT, class Iterator, class Function>
void join_print_as(StreamT& os, Iterator begin, Iterator end, llvm::StringRef separator, Function func)
{
    if (begin == end) {
        return;
    }

    func(os, *begin);

    while (++begin != end) {
        os << separator;
        func(os, *begin);
    }
}

}

#endif
