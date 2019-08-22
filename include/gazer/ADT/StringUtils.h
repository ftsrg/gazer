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
    if (begin != end) {
        os << *begin;
    }

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