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
#include "gazer/ADT/Iterator.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class Shape
{
public:
    enum Kind
    {
        Kind_Square,
        Kind_Circle
    };

    explicit Shape(Kind kind)
        : kind(kind)
    {}

    Kind kind;
};

class Circle : public Shape
{
public:
    explicit Circle(double r) : Shape(Kind_Circle), radius(r) {}

    static bool classof(const Shape* s)
    {
        return s->kind == Kind_Circle;
    }

    double radius;
};

class Square : public Shape
{
public:
    Square()
        : Shape(Kind_Square)
    {}

    static bool classof(const Shape* s)
    {
        return s->kind == Kind_Square;
    }
};

TEST(ClassOfIteratorTest, Simple)
{
    Square s1, s2;
    Circle c1(0.5);

    std::vector<Shape*> shapes = {&s1, &s2, &c1};

    int numElems = 0;
    for (const Circle* c : classof_range<Circle>(shapes.begin(), shapes.end())) {
        EXPECT_EQ(c->radius, 0.5);
        ++numElems;
    }

    ASSERT_EQ(numElems, 1);
}

TEST(ClassOfIteratorTest, References)
{
    // This should also work with references
    Square s1, s2;
    Circle c1(0.5);

    std::vector<Shape*> shapes = {&s1, &s2, &c1};

    int numElems = 0;
    for (const Circle& c : classof_range<Circle>(llvm::make_pointee_range(shapes))) {
        EXPECT_EQ(c.radius, 0.5);
        ++numElems;
    }
    ASSERT_EQ(numElems, 1);
}

} // namespace