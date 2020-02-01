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
#include <type_traits>
#include <bitset>

namespace gazer
{

template<class Enum>
inline constexpr bool enable_enum_flags_v = false;

/// A simple wrapper for enum bit flags.
template<class Enum>
class EnumSet
{
    // Some implementation details of this class are based on the following answer:
    //  https://softwareengineering.stackexchange.com/a/338472
    static_assert(std::is_enum_v<Enum>, "EnumSet may only be used with enum types!");

    using StorageT = std::underlying_type_t<Enum>;
    static constexpr int NumDigits = std::numeric_limits<StorageT>::digits;
public:
    constexpr EnumSet() = default;
    constexpr EnumSet(const EnumSet& other)
        : mData(other.mData)
    {}

    constexpr EnumSet& operator=(const EnumSet& other)
    {
        mData = other.mData;
        return *this;
    }

    constexpr EnumSet(Enum value)
        : mData(asBit(value))
    {}

    constexpr EnumSet<Enum> operator|(Enum value) const
    {
        EnumSet<Enum> result = *this;
        result.mData |= asBit(value);
        return result;
    }

    constexpr EnumSet<Enum> operator&(Enum value) const
    {
        EnumSet<Enum> result = *this;
        result.mData &= asBit(value);
        return result;
    }

    constexpr EnumSet<Enum> operator^(Enum value) const
    {
        EnumSet<Enum> result = *this;
        result.mData ^= asBit(value);
        return result;
    }

    constexpr EnumSet<Enum>& operator|=(Enum value)
    {
        mData |= asBit(value);
        return *this;
    }

    constexpr EnumSet<Enum>& operator&=(Enum value)
    {
        mData &= asBit(value);
        return *this;
    }

    constexpr EnumSet<Enum>& operator^=(Enum value)
    {
        mData ^= asBit(value);
        return *this;
    }


    constexpr bool any() const { return mData.any(); }
    constexpr bool none() const { return mData.none(); }

    // We do not provide all() as NumDigits is usually larger than
    // the number of enum members.
    constexpr bool all() const = delete;
    
    constexpr operator bool() { return any(); }

    constexpr bool test(Enum value) const { return mData.test(static_cast<StorageT>(value)); }

private:
    std::size_t asBit(Enum val) const { return 1 << static_cast<StorageT>(val); }

private:
    std::bitset<NumDigits> mData;
};

template<class Enum>
constexpr auto operator|(Enum left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>>
{
    using StorageT = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<StorageT>(left) | static_cast<StorageT>(right)
    );
}

template<class Enum>
constexpr auto operator&(Enum left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>>
{
    using StorageT = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<StorageT>(left) & static_cast<StorageT>(right)
    );
}

template<class Enum>
constexpr auto operator^(Enum left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>>
{
    using StorageT = std::underlying_type_t<Enum>;
    return static_cast<Enum>(
        static_cast<StorageT>(left) ^ static_cast<StorageT>(right)
    );
}

template<class Enum>
constexpr auto operator|=(Enum& left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>&>
{
    left = left | right;
    return left;
}

template<class Enum>
constexpr auto operator&=(Enum& left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>&>
{
    left = left & right;
    return left;
}

template<class Enum>
constexpr auto operator^=(Enum& left, Enum right)
    -> typename std::enable_if_t<enable_enum_flags_v<Enum>, EnumSet<Enum>&>
{
    left = left ^ right;
    return left;
}

} // end namespace gazer