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
#ifndef GAZER_SUPPORT_FLOAT_H
#define GAZER_SUPPORT_FLOAT_H

#include <cstdint>
#include <cstring>
#include <cassert>

namespace gazer
{

inline double double_from_uint64(uint64_t bits)
{
    double result;
    assert(sizeof(result) == sizeof(bits));
    memcpy(&result, &bits, sizeof(bits));

    return result;
}

inline float float_from_uint64(uint64_t bits)
{
    float result;
    uint32_t trunc = bits;
    assert(sizeof(trunc) == sizeof(bits));
    memcpy(&result, &bits, sizeof(trunc));

    return result;
}


}

#endif
