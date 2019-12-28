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
#ifndef GAZER_SUPPORT_MATH_H
#define GAZER_SUPPORT_MATH_H

namespace gazer::math
{

/// Calculates the power of two integers and returns an int.
/// Does not account for overflow and does not work for negative exponents.
unsigned ipow(unsigned base, unsigned exp)
{
    if (exp == 0) {
        return 1;
    }

    if (exp == 1) {
        return base;
    }

    unsigned tmp = ipow(base, exp / 2);
    if (exp % 2 == 0) {
        return tmp * tmp;
    }

    return base * tmp * tmp;
}

}

#endif
