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
#include "gazer/ADT/EnumSet.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

// Declare a simple enum class to test the container.
// This class has global enum set operators disabled as
// the container itself is supposed to work without those. 
enum class EnumClass
{
    A, B, C, D
};

namespace oldenums
{
    enum SimpleEnum
    {
        A, B, C, D
    };
}

enum class EnumClassOpsEnabled
{
    A, B, C, D
};

} // end anonymous namespace

namespace gazer
{

template<> inline constexpr bool enable_enum_flags_v<EnumClassOpsEnabled> = true;
template<> inline constexpr bool enable_enum_flags_v<oldenums::SimpleEnum> = true;

} // end namespace gazer

namespace
{

using ::testing::Types;

template<class T>
class EnumSetTest : public ::testing::Test {
};

using TestedTypes = ::testing::Types<EnumClass, oldenums::SimpleEnum, EnumClassOpsEnabled>;
TYPED_TEST_SUITE(EnumSetTest, TestedTypes);

TYPED_TEST(EnumSetTest, DefaultConstructAndAssign)
{
    // Start with an empty set
    EnumSet<TypeParam> set;
    ASSERT_TRUE(set.none());
    
    // Add a value
    set = TypeParam::A;
    
    // Check accessors
    ASSERT_TRUE(set.any());
    ASSERT_FALSE(set.none());

    ASSERT_TRUE(static_cast<bool>(set));

    // Check individual bits
    ASSERT_TRUE(set.test(TypeParam::A));
    ASSERT_FALSE(set.test(TypeParam::B));
    ASSERT_FALSE(set.test(TypeParam::C));
    ASSERT_FALSE(set.test(TypeParam::D));
}

TYPED_TEST(EnumSetTest, ConstructFromValue)
{
    EnumSet<TypeParam> set = TypeParam::A;
    
    ASSERT_TRUE(set.test(TypeParam::A));
    ASSERT_FALSE(set.test(TypeParam::B));
    ASSERT_FALSE(set.test(TypeParam::C));
    ASSERT_FALSE(set.test(TypeParam::D));
}

TYPED_TEST(EnumSetTest, AllTrue)
{
    EnumSet<TypeParam> set = TypeParam::A;
    set |= TypeParam::B;
    set |= TypeParam::C;
    set |= TypeParam::D;

    ASSERT_TRUE(set.any());
    ASSERT_FALSE(set.none());

    ASSERT_TRUE(set.test(TypeParam::A));
    ASSERT_TRUE(set.test(TypeParam::B));
    ASSERT_TRUE(set.test(TypeParam::C));
    ASSERT_TRUE(set.test(TypeParam::D));
}

}